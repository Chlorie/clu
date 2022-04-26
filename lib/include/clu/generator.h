#pragma once

// Rough implementation of P2502R1, sans allocator support
// std::generator: Synchronous Coroutine Generator for Ranges

#include <ranges>

#include "concepts.h"
#include "unique_coroutine_handle.h"
#include "macros.h"

namespace clu
{
    template <std::ranges::input_range R>
    struct elements_of_t
    {
        R range{};
    };

    template <typename R>
    elements_of_t(R&&) -> elements_of_t<R&&>;

    // Not to spec: elements_of is a factory function
    /**
     * \brief Wraps an input range to yield from in a generator coroutine.
     * \details This function wraps (forwards) a range into a special type,
     * yielding this type in a generator coroutine function is equivalent
     * to yielding every element in this range.
     * \param range The range to wrap.
     */
    template <std::ranges::input_range R>
    constexpr auto elements_of(R&& range) noexcept
    {
        return elements_of_t<R&&>{static_cast<R&&>(range)};
    }

    template <typename Ref, typename Value>
    class generator;

    namespace detail
    {
        template <typename Value, typename Yielded>
        class generator_iterator;

        template <typename T>
        class generator_promise_base
        {
        private:
            using handle_t = coro::coroutine_handle<generator_promise_base>;

            template <typename U>
            struct yield_lvalue_awaiter
            {
                U result;
                bool await_ready() const noexcept { return false; }
                template <typename P>
                void await_suspend(coro::coroutine_handle<P> handle) noexcept
                {
                    generator_promise_base& pms = handle.promise();
                    pms.ptr_ = std::addressof(result);
                }
                void await_resume() const noexcept {}
            };

            struct nested_info
            {
                std::exception_ptr exc{};
                handle_t parent{};
                handle_t root{};

                void maybe_rethrow() const
                {
                    if (exc)
                        std::rethrow_exception(exc);
                }

                void set_parent(generator_promise_base& pms) noexcept
                {
                    parent = handle_t::from_promise(pms);
                    if (pms.nested_)
                        root = pms.nested_->root;
                    else
                        root = parent;
                }
            };

            template <typename T2, typename V2>
            struct yield_generator_awaiter
            {
                generator<T2, V2> other;
                nested_info info;

                // If the generator is empty, don't bother resuming
                bool await_ready() const noexcept { return other.handle_.done(); }

                template <typename P>
                coro::coroutine_handle<> await_suspend(coro::coroutine_handle<P> handle) noexcept
                {
                    generator_promise_base& pms = handle.promise();
                    generator_promise_base& other_pms = other.handle_.promise();
                    pms.inner_most_ = handle_t::from_promise(other_pms);
                    info.set_parent(pms);
                    other_pms.nested_ = &info;
                    return other.handle_.get(); // resume the inner generator
                }

                void await_resume() const noexcept { info.maybe_rethrow(); }
            };

            struct final_awaiter
            {
                bool await_ready() const noexcept { return false; }

                template <typename P>
                coro::coroutine_handle<> await_suspend(coro::coroutine_handle<P> handle) const noexcept
                {
                    if (generator_promise_base& pms = handle.promise(); pms.nested_)
                    {
                        // The iterator should resume the outer generator next
                        pms.nested_->root.promise().inner_most_ = pms.nested_->parent;
                        return pms.nested_->parent;
                    }
                    // We are already the outermost generator, just suspend
                    return coro::noop_coroutine();
                }

                void await_resume() const noexcept {}
            };

        public:
            coro::suspend_always initial_suspend() const noexcept { return {}; }
            final_awaiter final_suspend() const noexcept { return {}; }

            coro::suspend_always yield_value(T value) noexcept
            {
                ptr_ = std::addressof(value);
                return {};
            }

            // clang-format off
            // When the yielded type is an rvalue and the user co_yield-s an lvalue,
            // copy the lvalue into an awaiter and yield that as an rvalue
            template <typename U = std::remove_cvref_t<T>> requires
                std::is_rvalue_reference_v<T> &&
                std::constructible_from<std::remove_cvref_t<T>, const U&>
            auto yield_value(const std::type_identity_t<U>& value)
            // clang-format on
            {
                return yield_lvalue_awaiter<U>{value};
            }

            template <class T2, class V2>
                requires std::same_as<typename generator<T2, V2>::yielded, T>
            auto yield_value(elements_of_t<generator<T2, V2>&&> other) noexcept
            {
                return yield_generator_awaiter<T2, V2>{std::move(other.range), {}};
            }

            // Not to spec: supports yielding ranges with lvalue references
            // in an rvalue generator
            // clang-format off
            template <std::ranges::input_range Rng> requires
                (std::convertible_to<std::ranges::range_reference_t<Rng>, T>) ||
                (std::is_rvalue_reference_v<T> &&
                    std::constructible_from<std::remove_cvref_t<T>, std::ranges::range_reference_t<Rng>>)
            auto yield_value(elements_of_t<Rng> range) noexcept
            // clang-format on
            {
                return this->yield_value(clu::elements_of(
                    [](Rng r) -> generator<T, std::ranges::range_value_t<Rng>>
                    {
                        for (auto&& e : static_cast<Rng>(r))
                            co_yield static_cast<decltype(e)>(e);
                    }(static_cast<Rng>(range.range))));
            }

            void return_void() const noexcept {}

            void unhandled_exception() const
            {
                if (nested_)
                    nested_->exc = std::current_exception();
                else
                    throw;
            }

        private:
            using pointer_t = std::add_pointer_t<T>;

            template <typename, typename>
            friend class generator_iterator;

            handle_t inner_most_ = handle_t::from_promise(*this);
            nested_info* nested_ = nullptr;
            pointer_t ptr_ = nullptr;
        };

        template <typename Value, typename Yielded>
        class generator_iterator
        {
        public:
            using value_type = Value;
            using difference_type = std::ptrdiff_t;

            // clang-format off
            template <typename P>
            explicit generator_iterator(coro::coroutine_handle<P> handle) noexcept:
                handle_(handle_t::from_promise(handle.promise())) {}
            // clang-format on

            CLU_NON_COPYABLE_TYPE(generator_iterator);
            CLU_DEFAULT_MOVE_MEMBERS(generator_iterator);

            Yielded operator*() const noexcept
            {
                return static_cast<Yielded>(*handle_.promise().inner_most_.promise().ptr_);
            }

            generator_iterator& operator++() noexcept
            {
                handle_.promise().inner_most_.resume();
                return *this;
            }

            void operator++(int) noexcept { ++*this; }

            bool operator==(std::default_sentinel_t) const noexcept { return handle_.done(); }

        private:
            using handle_t = coro::coroutine_handle<generator_promise_base<Yielded>>;
            coro::coroutine_handle<generator_promise_base<Yielded>> handle_{};
        };
    } // namespace detail

    template <typename Ref, typename Value = void>
    class generator
    {
    private:
        using value_t = conditional_t<std::is_void_v<Value>, std::remove_cvref_t<Ref>, Value>;
        using reference_t = conditional_t<std::is_void_v<Value>, Ref&&, Ref>;
        using rvalue_ref_t =
            conditional_t<std::is_reference_v<reference_t>, std::remove_reference_t<reference_t>&&, reference_t>;

        // clang-format off
        // Mandates:
        static_assert(no_cvref_v<value_t>);
        static_assert(
            std::is_reference_v<reference_t> ||
            (no_cvref_v<reference_t> && std::copy_constructible<reference_t>));
        static_assert(std::common_reference_with<reference_t&&, value_t&>);
        static_assert(std::common_reference_with<reference_t&&, rvalue_ref_t>);
        static_assert(std::common_reference_with<rvalue_ref_t, const value_t&>);
        // clang-format on

    public:
        using yielded = conditional_t<std::is_reference_v<reference_t>, reference_t, const reference_t&>;

        struct promise_type : detail::generator_promise_base<yielded>
        {
            generator get_return_object() noexcept
            {
                const auto handle = coro::coroutine_handle<promise_type>::from_promise(*this);
                return generator(handle);
            }
        };

        auto begin() noexcept
        {
            using iterator = detail::generator_iterator<value_t, yielded>;
            handle_.resume();
            return iterator(handle_.get());
        }
        std::default_sentinel_t end() const noexcept { return {}; }

    private:
        template <typename T>
        friend class detail::generator_promise_base;

        unique_coroutine_handle<promise_type> handle_;
        explicit generator(const coro::coroutine_handle<promise_type> handle) noexcept: handle_(handle) {}
    };
} // namespace clu

template <typename Ref, typename Value>
inline constexpr bool std::ranges::enable_view<clu::generator<Ref, Value>> = true;
