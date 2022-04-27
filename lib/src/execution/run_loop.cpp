#include "clu/execution/run_loop.h"

namespace clu::detail::loop
{
    run_loop::~run_loop() noexcept
    {
        if (head_ || !stopped_)
            std::terminate();
    }

    void run_loop::run()
    {
        while (ops_base* task = dequeue())
            task->execute();
    }

    void run_loop::finish()
    {
        std::unique_lock lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }

    void tag_invoke(exec::add_operation_t, run_loop& self, run_loop::ops_base& task)
    {
        {
            std::unique_lock lock(self.mutex_);
            self.tail_ = (self.head_ ? self.tail_->next : self.head_) = &task;
        }
        self.cv_.notify_one();
    }

    run_loop::ops_base* run_loop::dequeue()
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return head_ != nullptr || stopped_; });
        if (!head_)
            return nullptr;
        ops_base* ptr = head_;
        head_ = head_->next;
        if (!head_)
            tail_ = nullptr;
        return ptr;
    }
} // namespace clu::detail::loop
