#pragma once

#include <mutex>
#include <condition_variable>

#include "execution_traits.h"
#include "context.h"

namespace clu
{
    namespace detail::loop
    {
        CLU_SUPPRESS_EXPORT_WARNING
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
            using ops_base = exec::context_operation_state_base<run_loop>;

            enum class state_t
            {
                starting,
                running,
                finishing
            };

            std::mutex mutex_;
            std::condition_variable cv_;

            bool stopped_ = false;
            ops_base* head_ = nullptr;
            ops_base* tail_ = nullptr;

            CLU_API friend void tag_invoke(exec::add_operation_t, run_loop& self, ops_base& task);
            ops_base* dequeue();
        };
        CLU_RESTORE_EXPORT_WARNING
    } // namespace detail::loop

    using detail::loop::run_loop;
} // namespace clu
