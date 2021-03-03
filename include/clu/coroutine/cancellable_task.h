#pragma once

#include <atomic>

#include "unique_coroutine_handle.h"
#include "concepts.h"
#include "../outcome.h"

namespace clu
{
    template <typename T> class cancellable_task;

    struct is_cancelled_t {};
    inline constexpr is_cancelled_t is_cancelled{};

    namespace detail
    {
        class cancellable_task_promise_base
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

            using cancel_awaiting_t = void(*)(void*);
            static constexpr unsigned not_awaiting = 0;
            static constexpr unsigned awaiting = 1;
            static constexpr unsigned cancelled = 2;

            template <typename A>
            struct transformed_awaitable final
            {
                cancellable_task_promise_base& parent;
                A&& awaitable;

                bool await_ready() const { return awaitable.await_ready(); }

                auto await_suspend(const std::coroutine_handle<> handle) const
                {
                    parent.awaiting_ = const_cast<void*>(static_cast<const void*>(std::addressof(awaitable)));
                    parent.cancel_fptr_ = [](void* ptr)
                    {
                        auto aptr = static_cast<std::add_pointer_t<A>>(ptr);
                        aptr->cancel();
                    };

                    const auto result = [&]
                    {
                        using type = decltype(awaitable.await_suspend(handle));
                        if constexpr (std::is_void_v<type>)
                        {
                            awaitable.await_suspend(handle);
                            return true;
                        }
                        else
                            return awaitable.await_suspend(handle);
                    }();
                    unsigned expected = not_awaiting;
                    if (!parent.state_.compare_exchange_strong(expected, awaiting,
                        std::memory_order_release, std::memory_order_relaxed)) // cancelled
                        parent.cancel_fptr_(parent.awaiting_);

                    return result;
                }

                decltype(auto) await_resume() const
                {
                    unsigned expected = awaiting;
                    parent.state_.compare_exchange_strong(expected, not_awaiting,
                        std::memory_order_acquire, std::memory_order_acquire);
                    return awaitable.await_resume();
                }
            };

            std::coroutine_handle<> cont_{};
            void* awaiting_ = nullptr;
            cancel_awaiting_t cancel_fptr_ = nullptr;
            std::atomic_uint state_ = not_awaiting;

        public:
            std::suspend_always initial_suspend() const noexcept { return {}; }
            final_awaitable final_suspend() const noexcept { return {}; }

            void set_continuation(const std::coroutine_handle<> cont) noexcept { cont_ = cont; }

            void cancel()
            {
                const unsigned old = state_.exchange(cancelled, std::memory_order_acquire);
                if (old == awaiting)
                    cancel_fptr_(awaiting_);
            }

            template <cancellable_awaitable A>
            transformed_awaitable<A> await_transform(A&& awaitable) { return { *this, std::forward<A>(awaitable) }; }

            template <awaitable A>
            auto&& await_transform(A&& awaitable) { return std::forward<A>(awaitable); }

            auto await_transform(is_cancelled_t) const
            {
                struct awaiter
                {
                    const cancellable_task_promise_base& promise;
                    bool await_ready() const noexcept { return true; }
                    void await_suspend(std::coroutine_handle<>) const {}
                    bool await_resume() const noexcept { return promise.state_.load(std::memory_order_acquire) != cancelled; }
                };
                return awaiter(*this);
            }
        };

        template <typename T>
        class cancellable_task_promise final : public cancellable_task_promise_base
        {
        private:
            outcome<T> result_;

        public:
            cancellable_task<T> get_return_object();

            template <typename U = T> requires std::convertible_to<U&&, T>
            void return_value(U&& value) { result_ = std::forward<U>(value); }
            void unhandled_exception() noexcept { result_ = std::current_exception(); }

            T get() { return *std::move(result_); }
        };

        template <>
        class cancellable_task_promise<void> final : public cancellable_task_promise_base
        {
        private:
            std::exception_ptr eptr_;

        public:
            cancellable_task<void> get_return_object();

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
    class [[nodiscard]] cancellable_task final
    {
    public:
        using promise_type = detail::cancellable_task_promise<T>;

    private:
        unique_coroutine_handle<promise_type> handle_{};

    public:
        explicit cancellable_task(promise_type& promise):
            handle_(std::coroutine_handle<promise_type>::from_promise(promise)) {}

        bool await_ready() const noexcept { return false; }

        std::coroutine_handle<> await_suspend(const std::coroutine_handle<> awaiting)
        {
            handle_.get().promise().set_continuation(awaiting);
            return handle_.get();
        }

        T await_resume() { return handle_.get().promise().get(); }

        void cancel() const { handle_.get().promise().cancel(); }
    };

    namespace detail
    {
        template <typename T> cancellable_task<T> cancellable_task_promise<T>::get_return_object() { return cancellable_task<T>(*this); }
        inline cancellable_task<> cancellable_task_promise<void>::get_return_object() { return cancellable_task<>(*this); }
    }

    template <cancellable_awaitable T>
    cancellable_task<await_result_t<T&&>> make_cancellable_task(T&& awaitable) { co_return co_await std::forward<T>(awaitable); }
}
