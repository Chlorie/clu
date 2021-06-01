#pragma once

#include "clu/unique_coroutine_handle.h"
#include "../outcome.h"
#include "concepts.h"

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
                std::coroutine_handle<> await_suspend(const std::coroutine_handle<P> handle) const noexcept
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
            outcome<T> result_;

        public:
            task<T> get_return_object();

            template <typename U = T> requires std::convertible_to<U&&, T>
            void return_value(U&& value) { result_ = std::forward<U>(value); }
            void unhandled_exception() noexcept { result_ = std::current_exception(); }

            T get() { return *std::move(result_); }
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
    class [[nodiscard]] task final
    {
    public:
        using promise_type = detail::task_promise<T>;

    private:
        unique_coroutine_handle<promise_type> handle_{};

    public:
        explicit task(promise_type& promise): handle_(std::coroutine_handle<promise_type>::from_promise(promise)) {}

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(const std::coroutine_handle<> awaiting)
        {
            handle_.get().promise().set_continuation(awaiting);
            return handle_.get();
        }

        T await_resume() { return handle_.get().promise().get(); }
    };

    namespace detail
    {
        template <typename T> task<T> task_promise<T>::get_return_object() { return task<T>(*this); }
        inline task<> task_promise<void>::get_return_object() { return task<>(*this); }
    }

    template <awaitable T>
    task<await_result_t<T&&>> make_task(T&& awaitable) { co_return co_await std::forward<T>(awaitable); }
}
