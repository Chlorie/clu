#pragma once

#include <memory>
#include <thread>

#include "../execution/run_loop.h"

namespace clu
{
    CLU_SUPPRESS_EXPORT_WARNING
    class CLU_API single_thread_context
    {
    public:
        single_thread_context(): thr_([this] { loop_.run(); }) {}
        ~single_thread_context() noexcept { finish(); }
        single_thread_context(const single_thread_context&) = delete;
        single_thread_context(single_thread_context&&) = delete;
        single_thread_context& operator=(const single_thread_context&) = delete;
        single_thread_context& operator=(single_thread_context&&) = delete;

        auto get_scheduler() noexcept { return loop_.get_scheduler(); }

        void finish()
        {
            loop_.finish();
            if (thr_.joinable())
                thr_.join();
        }

        std::thread::id get_id() const noexcept { return thr_.get_id(); }

    private:
        run_loop loop_;
        std::thread thr_;
    };

    namespace detail::static_tp
    {
        class CLU_API pool
        {
        public:
            explicit pool(std::size_t size);
            pool(const pool&) = delete;
            pool(pool&&) = delete;
            pool& operator=(const pool&) = delete;
            pool& operator=(pool&&) = delete;

            ~pool() noexcept;
            void finish();

        private:
            struct schd_state
            {
                pool* ctx;
                friend bool operator==(schd_state, schd_state) noexcept = default;
            };
            struct ops_state;
            using ops_base = exec::scheduler_operation_base<ops_state, schd_state>;
            struct ops_state
            {
                ops_base* next = nullptr;
            };

        public:
            auto get_scheduler() noexcept
            {
                return exec::create_scheduler<ops_state>(schd_state{this}, //
                    [](ops_base& ops) { return ops.scheduler_state().ctx->enqueue(&ops); });
            }

        private:
            class thread_res;

            std::size_t size_ = 0;
            thread_res* res_ = nullptr;
            std::atomic_size_t index_ = 0;

            void enqueue(ops_base* task);
            void work(std::size_t index);
        };
    } // namespace detail::static_tp
    CLU_RESTORE_EXPORT_WARNING

    using static_thread_pool = detail::static_tp::pool;
} // namespace clu
