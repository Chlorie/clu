#pragma once

#include <coroutine>
#include <exception>
#include <ranges>

#include "unique_coroutine_handle.h"

namespace clu
{
    template <typename T> class generator;

    namespace detail
    {
        class generator_promise_base
        {
        private:
            std::exception_ptr eptr_;

        protected:
            void throw_if_needed() const
            {
                if (eptr_)
                    std::rethrow_exception(eptr_);
            }

        public:
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_always final_suspend() const noexcept { return {}; }
            void unhandled_exception() noexcept { eptr_ = std::current_exception(); }
            void return_void() const noexcept {}
        };

        template <typename T>
        class generator_promise final : public generator_promise_base
        {
        private:
            T value_{};

        public:
            generator<T> get_return_object();

            template <typename U> requires std::assignable_from<T&, U&&>
            std::suspend_always yield_value(U&& value)
            {
                throw_if_needed();
                value_ = std::forward<U>(value);
                return {};
            }

            T& get() { return value_; }
        };

        template <typename T>
        class generator_promise<T&> final : public generator_promise_base
        {
        private:
            T* value_ = nullptr;

        public:
            generator<T&> get_return_object();
            std::suspend_always yield_value(T& value)
            {
                throw_if_needed();
                value_ = std::addressof(value);
                return {};
            }

            T& get() { return *value_; }
        };

        struct generator_sentinel final {};

        template <typename T>
        class generator_iterator final
        {
        public:
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using iterator_concept = std::input_iterator_tag;

        private:
            using handle_t = std::coroutine_handle<generator_promise<T>>;
            handle_t handle_{};

        public:
            generator_iterator() = default;
            explicit generator_iterator(const handle_t handle): handle_(handle) {}

            T& operator*() const { return handle_.promise().get(); }

            generator_iterator& operator++()
            {
                handle_.resume();
                return *this;
            }

            generator_iterator operator++(int)
            {
                generator_iterator result = *this;
                operator++();
                return result;
            }

            friend bool operator==(const generator_iterator it, generator_sentinel) { return it.handle_.done(); }
        };
    }

    template <typename T>
    class generator final
    {
    public:
        using promise_type = detail::generator_promise<T>;
        using iterator = detail::generator_iterator<T>;
        using sentinel = detail::generator_sentinel;

    private:
        unique_coroutine_handle<promise_type> handle_{};

    public:
        explicit generator(promise_type& promise):
            handle_(std::coroutine_handle<promise_type>::from_promise(promise))
        {
            static_assert(std::input_iterator<iterator>);
            static_assert(std::ranges::input_range<generator>);
        }

        iterator begin() noexcept
        {
            handle_.get().resume();
            return iterator(handle_.get());
        }
        sentinel end() const noexcept { return {}; }

        decltype(auto) next() const
        {
            handle_.get().resume();
            return handle_.get().promise().get();
        }
        decltype(auto) operator()() const { return next(); }
        bool done() const { return handle_.get().done(); }
    };

    namespace detail
    {
        template <typename T> generator<T> generator_promise<T>::get_return_object() { return generator<T>(*this); }
        template <typename T> generator<T&> generator_promise<T&>::get_return_object() { return generator<T&>(*this); }
    }
}
