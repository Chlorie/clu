#pragma once

#include <mutex>
#include <condition_variable>

#include "factories.h"

namespace clu
{
    CLU_SUPPRESS_EXPORT_WARNING
    class CLU_API run_loop
    {
    public:
        run_loop() noexcept = default;
        ~run_loop() noexcept;
        CLU_IMMOVABLE_TYPE(run_loop);

        void run();
        void finish();

    private:
        struct schd_state
        {
            run_loop* loop;
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
                [](ops_base& ops) { ops.scheduler_state().loop->enqueue(ops); });
        }

    private:
        std::mutex mutex_;
        std::condition_variable cv_;

        bool stopped_ = false;
        ops_base* head_ = nullptr;
        ops_base* tail_ = nullptr;

        void enqueue(ops_base& ops);
        ops_base* dequeue();
    };
    CLU_RESTORE_EXPORT_WARNING
} // namespace clu
