#pragma once

#include <coroutine>
#include <exception>
#include <ranges>

#include "outcome.h"
#include "../unique_coroutine_handle.h"
#include "../iterator.h"

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
        class generator_iterator_impl
        {
        private:
            using handle_t = std::coroutine_handle<generator_promise<T>>;
            handle_t handle_{};

        public:
            generator_iterator_impl() = default;
            explicit generator_iterator_impl(const handle_t handle): handle_(handle) {}

            T& operator*() const { return handle_.promise().get(); }

            generator_iterator_impl& operator++()
            {
                handle_.resume();
                return *this;
            }

            friend bool operator==(const generator_iterator_impl it, generator_sentinel) { return it.handle_.done(); }
        };
    }

    template <typename T>
    class generator final
    {
    public:
        using promise_type = detail::generator_promise<T>;
        using iterator = iterator_adapter<detail::generator_iterator_impl<T>>;
        using sentinel = detail::generator_sentinel;

    private:
        unique_coroutine_handle<promise_type> handle_{};

    public:
        explicit generator(promise_type& promise):
            handle_(std::coroutine_handle<promise_type>::from_promise(promise)) {}

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
