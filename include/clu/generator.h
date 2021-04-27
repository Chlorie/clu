#pragma once

#include <coroutine>
#include <exception>
#include <ranges>

#include "unique_coroutine_handle.h"
#include "outcome.h"

namespace clu
{
    template <typename T> class generator;

    namespace detail
    {
        template <typename T>
        class generator_promise final
        {
        private:
            outcome<T> value_;

        public:
            generator<T> get_return_object();
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_always final_suspend() const noexcept { return {}; }
            void return_void() const noexcept {}
            void unhandled_exception() noexcept { value_ = std::current_exception(); }

            template <typename U = T> requires std::assignable_from<T&, U&&>
            std::suspend_always yield_value(U&& value)
            {
                value_.throw_if_exceptional();
                value_ = std::forward<U>(value);
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
    }
}
