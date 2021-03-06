#pragma once

#include <mutex>
#include <condition_variable>

#include "concepts.h"
#include "../outcome.h"

namespace clu
{
    namespace detail
    {
        template <typename T> class sync_wait_task;

        template <typename T>
        class sync_wait_task_promise final
        {
        private:
            std::mutex mutex_;
            std::condition_variable cv_;
            outcome<T> result_;

        public:
            sync_wait_task<T> get_return_object();
            std::suspend_never initial_suspend() const noexcept { return {}; }
            std::suspend_always final_suspend() const noexcept { return {}; }

            template <typename U>
            void return_value(U&& value)
            {
                {
                    std::scoped_lock lock(mutex_);
                    result_ = std::forward<U>(value);
                }
                cv_.notify_one();
            }

            void unhandled_exception()
            {
                {
                    std::scoped_lock lock(mutex_);
                    result_ = std::current_exception();
                }
                cv_.notify_one();
            }

            decltype(auto) get() &&
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this]() { return !result_.empty(); });
                return *std::move(result_);
            }
        };

        template <>
        class sync_wait_task_promise<void> final
        {
        public:
            sync_wait_task<void> get_return_object();
            std::suspend_never initial_suspend() const noexcept { return {}; }
            std::suspend_always final_suspend() const noexcept { return {}; }

#ifdef __cpp_lib_atomic_wait
        private:
            std::atomic_bool done_{ false };
            std::exception_ptr eptr_;

        public:
            void return_void()
            {
                done_.store(true, std::memory_order_release);
                done_.notify_one();
            }

            void unhandled_exception()
            {
                eptr_ = std::current_exception();
                done_.store(true, std::memory_order_release);
                done_.notify_one();
            }

            void get() &&
            {
                done_.wait(false, std::memory_order_acquire);
                if (eptr_) std::rethrow_exception(eptr_);
            }
#else // gcc trunk atomic wait workaround 
        private:
            std::mutex mutex_;
            std::condition_variable cv_;
            outcome<void> result_;

        public:
            void return_void()
            {
                {
                    std::scoped_lock lock(mutex_);
                    result_ = void_tag;
                }
                cv_.notify_one();
            }

            void unhandled_exception()
            {
                {
                    std::scoped_lock lock(mutex_);
                    result_ = std::current_exception();
                }
                cv_.notify_one();
            }

            void get() &&
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this]() { return !result_.empty(); });
                return *result_;
            }
#endif
        };

        template <typename T>
        class sync_wait_task final // NOLINT(cppcoreguidelines-special-member-functions)
        {
        public:
            using promise_type = sync_wait_task_promise<T>;

        private:
            using handle_t = std::coroutine_handle<promise_type>;
            std::coroutine_handle<promise_type> handle_{};

        public:
            explicit sync_wait_task(promise_type& promise): handle_(handle_t::from_promise(promise)) {}
            ~sync_wait_task() noexcept { handle_.destroy(); }

            decltype(auto) get() { return std::move(handle_.promise()).get(); }
        };

        template <typename T> sync_wait_task<T> sync_wait_task_promise<T>::get_return_object() { return sync_wait_task<T>(*this); }
        inline sync_wait_task<void> sync_wait_task_promise<void>::get_return_object() { return sync_wait_task<void>(*this); }
    }

    template <awaitable A>
    await_result_t<A&&> sync_wait(A&& awt)
    {
        auto task = [&]() -> detail::sync_wait_task<await_result_t<A&&>>
        {
            co_return co_await std::forward<A>(awt);
        }();
        return task.get();
    }
}
