#pragma once

#include <coroutine>
#include <variant>
#include <concepts>

#include "unique_coroutine_handle.h"
#include "../detail/value_wrapper.h"

namespace clu
{
    template <typename T> class task;

    namespace detail
    {
        class task_promise_base
        {
        private:
            struct final_awaitable final
            {
                bool await_ready() const noexcept { return false; }
                template <typename P>
                std::coroutine_handle<> await_suspend(const std::coroutine_handle<P> handle) const
                {
                    return handle.promise().cont_;
                }
                void await_resume() const noexcept {}
            };

            std::coroutine_handle<> cont_{};

        public:
            std::suspend_always initial_suspend() const noexcept { return {}; }
            final_awaitable final_suspend() const noexcept { return {}; }

            void set_continuation(const std::coroutine_handle<> cont) noexcept { cont_ = cont; }
        };

        template <typename T>
        class task_promise final : public task_promise_base
        {
        private:
            value_wrapper<T> result_;

        public:
            task<T> get_return_object();

            template <typename U> requires std::convertible_to<U&&, T>
            void return_value(U&& value) { result_.emplace(std::forward<U>(value)); }
            void unhandled_exception() noexcept { result_.capture_exception(); }

            decltype(auto) get() & { return result_.get(); }
            decltype(auto) get() const & { return result_.get(); }
            decltype(auto) get() && { return std::move(result_).get(); }
            decltype(auto) get() const && { return std::move(result_).get(); }
        };

        template <>
        class task_promise<void> final : public task_promise_base
        {
        private:
            std::exception_ptr eptr_;

        public:
            task<void> get_return_object();

            void return_void() const noexcept {}
            void unhandled_exception() noexcept { eptr_ = std::current_exception(); }

            void get() const
            {
                if (eptr_)
                    std::rethrow_exception(eptr_);
            }
        };
    }

    template <typename T = void>
    class task final
    {
    public:
        using promise_type = detail::task_promise<T>;

    private:
        using handle_t = std::coroutine_handle<promise_type>;

        class awaiter_base
        {
        protected:
            handle_t handle_{};

        public:
            explicit awaiter_base(const handle_t handle): handle_(handle) {}

            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(const std::coroutine_handle<> awaiting)
            {
                handle_.promise().set_continuation(awaiting);
                return handle_;
            }
        };

        unique_coroutine_handle<promise_type> handle_{};

    public:
        explicit task(promise_type& promise): handle_(handle_t::from_promise(promise)) {}

        auto operator co_await() const &
        {
            class task_awaiter final : public awaiter_base
            {
            public:
                using awaiter_base::awaiter_base;
                decltype(auto) await_resume() { return awaiter_base::handle_.promise().get(); }
            };
            return task_awaiter(handle_.get());
        }

        auto operator co_await() const &&
        {
            class task_awaiter final : public awaiter_base
            {
            public:
                using awaiter_base::awaiter_base;
                decltype(auto) await_resume() { return std::move(awaiter_base::handle_.promise()).get(); }
            };
            return task_awaiter(handle_.get());
        }
    };

    namespace detail
    {
        template <typename T> task<T> task_promise<T>::get_return_object() { return task<T>(*this); }
        inline task<> task_promise<void>::get_return_object() { return task<>(*this); }
    }
}
