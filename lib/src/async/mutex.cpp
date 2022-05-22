#include "clu/async/mutex.h"

namespace clu::async
{
    using detail::mtx::ops_base;

    bool mutex::try_lock() noexcept
    {
        void* expected = this;
        return waiting_.compare_exchange_weak(
            expected, nullptr, std::memory_order::acquire, std::memory_order::relaxed);
    }

    void mutex::unlock() noexcept
    {
        if (!pending_)
        {
            void* expected = waiting_.load(std::memory_order::relaxed);
            while (true)
            {
                if (expected == nullptr)
                {
                    // No one is waiting
                    if (waiting_.compare_exchange_weak(
                            expected, this, std::memory_order::release, std::memory_order::relaxed))
                        return;
                }
                else
                {
                    // Clear the waiting queue
                    if (waiting_.compare_exchange_weak(
                            expected, nullptr, std::memory_order::release, std::memory_order::relaxed))
                        break;
                }
            }
            // Transfer everything from the waiting queue to the pending queue
            auto* head = static_cast<ops_base*>(expected);
            ops_base* prev = nullptr;
            while (head)
            {
                pending_ = std::exchange(head, head->next);
                pending_->next = std::exchange(prev, pending_);
            }
        }
        // Now pending is not null, wake up the next waiter
        std::exchange(pending_, pending_->next)->set();
    }

    bool start_ops(mutex& self, ops_base& ops) noexcept
    {
        void* expected = self.waiting_.load(std::memory_order::relaxed);
        while (true)
        {
            if (expected == &self) // Try to acquire lock synchronously
            {
                if (self.waiting_.compare_exchange_weak(
                        expected, nullptr, std::memory_order::acquire, std::memory_order::relaxed))
                    return false; // Acquired the lock synchronously
            }
            else
            {
                ops.next = static_cast<ops_base*>(expected);
                if (self.waiting_.compare_exchange_weak(
                        expected, &ops, std::memory_order::acquire, std::memory_order::relaxed))
                    return true; // Enqueued ops into the waiting queue
            }
        }
    }
} // namespace clu::async
