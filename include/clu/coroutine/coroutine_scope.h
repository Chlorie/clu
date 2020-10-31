#pragma once

#include <atomic>

#include "oneway_task.h"
#include "schedule.h"
#include "sync_wait.h"
#include "../scope.h"

namespace clu
{
    class coroutine_scope final
    {
    private:
        class join_awaiter final
        {
        private:
            coroutine_scope* scope_;

        public:
            explicit join_awaiter(coroutine_scope* scope): scope_(scope) {}

            bool await_ready() const noexcept { return scope_->outstanding_.load(std::memory_order_acquire) == 0; }

            bool await_suspend(const std::coroutine_handle<> coro) const noexcept
            {
                scope_->continuation_ = coro;
                return scope_->outstanding_.fetch_sub(1, std::memory_order_acq_rel) > 1;
            }

            void await_resume() const noexcept {}
        };

        std::atomic_size_t outstanding_{ 1 };
        std::coroutine_handle<> continuation_{};

        void on_coro_start() noexcept { outstanding_.fetch_add(1, std::memory_order_relaxed); }
        void on_coro_complete()
        {
            if (outstanding_.fetch_sub(1, std::memory_order_acq_rel) == 1)
                continuation_.resume();
        }

    public:
        coroutine_scope() = default;
        coroutine_scope(const coroutine_scope&) = delete;
        coroutine_scope(coroutine_scope&&) = delete;
        coroutine_scope& operator=(const coroutine_scope&) = delete;
        coroutine_scope& operator=(coroutine_scope&&) = delete;

        ~coroutine_scope() noexcept
        {
            if (!continuation_)
                sync_wait(join());
        }

        template <awaitable T>
        void spawn(T&& awaitable)
        {
            [](coroutine_scope* scope, std::remove_cvref_t<T> awt) -> oneway_task
            {
                scope->on_coro_start();
                scope_exit exit([scope]() { scope->on_coro_complete(); });
                co_await std::move(awt);
            }(this, std::forward<T>(awaitable));
        }

        template <awaitable T, scheduler S>
        void spawn_on(T&& awaitable, S& scheduler)
        {
            [](coroutine_scope* scope, S& sch, std::remove_cvref_t<T> awt) -> oneway_task
            {
                scope->on_coro_start();
                scope_exit exit([scope]() { scope->on_coro_complete(); });
                co_await sch.schedule();
                co_await std::move(awt);
            }(this, scheduler, std::forward<T>(awaitable));
        }

        [[nodiscard]] join_awaiter join() noexcept { return join_awaiter(this); }
    };
}
