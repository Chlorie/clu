#pragma once

#include <mutex>
#include <condition_variable>

#include "execution_traits.h"
#include "context.h"

namespace clu
{
    namespace detail::loop
    {
        class CLU_API run_loop
        {
        public:
            run_loop() noexcept = default;
            run_loop(const run_loop&) = delete;
            run_loop(run_loop&&) = delete;
            run_loop& operator=(const run_loop&) = delete;
            run_loop& operator=(run_loop&&) = delete;

            ~run_loop() noexcept;
            void run();
            void finish();
            auto get_scheduler() noexcept { return exec::context_scheduler<run_loop>(this); }

        private:
            using ops_base = exec::operation_state_base<run_loop>;

            enum class state_t
            {
                starting,
                running,
                finishing
            };

            std::mutex mutex_;
            std::condition_variable cv_;

            state_t state_ = state_t::starting;
            ops_base* head_ = nullptr;
            ops_base* tail_ = nullptr;

            friend void tag_invoke(exec::add_operation_t, run_loop& self, ops_base& task) { self.enqueue(&task); }
            void enqueue(ops_base* task);
            ops_base* dequeue();
        };
    } // namespace detail::loop

    using detail::loop::run_loop;
} // namespace clu
