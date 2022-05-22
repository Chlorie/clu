#include "clu/async/shared_mutex.h"

#include <mutex>

namespace clu::async
{
    using detail::shmtx::ops_base;

    bool shared_mutex::try_lock() noexcept
    {
        const std::unique_lock lock(mut_, std::try_to_lock);
        if (lock && shared_holder_ == 0) // Not locked by anyone
        {
            shared_holder_ = unique_locked;
            return true;
        }
        return false;
    }

    void shared_mutex::unlock() noexcept
    {
        auto* res = [this]
        {
            std::unique_lock lock(mut_);
            CLU_ASSERT(shared_holder_ == unique_locked, //
                "Trying to uniquely unlock a shared mutex but not holding the lock currently");
            shared_holder_ = 0;
            return get_resumption_ops();
        }();
        while (res)
        {
            res->set();
            res = res->next;
        }
    }

    bool shared_mutex::try_lock_shared() noexcept
    {
        const std::unique_lock lock(mut_, std::try_to_lock);
        // Not uniquely locked, also no unique lock is waiting
        if (lock && shared_holder_ != unique_locked && !waiting_ && !pending_)
        {
            shared_holder_++;
            return true;
        }
        return false;
    }

    void shared_mutex::unlock_shared() noexcept
    {
        auto* res = [this]() -> ops_base*
        {
            std::unique_lock lock(mut_);
            CLU_ASSERT(shared_holder_ > 0, //
                "Trying to shared unlock a shared mutex but not holding the lock currently");
            CLU_ASSERT(shared_holder_ != unique_locked,
                "Trying to shared unlock a shared mutex but the mutex is currently locked uniquely");
            if (--shared_holder_ == 0) // Last shared unlock
                return get_resumption_ops();
            return nullptr;
        }();
        while (res)
        {
            res->set();
            res = res->next;
        }
    }

    bool start_ops(shared_mutex& self, ops_base& ops) noexcept
    {
        if (ops.shared)
            return self.enqueue_shared(ops);
        else
            return self.enqueue_unique(ops);
    }

    bool shared_mutex::enqueue_unique(ops_base& ops) noexcept
    {
        std::unique_lock lock(mut_);
        if (shared_holder_ == 0)
        {
            shared_holder_ = unique_locked;
            return false; // Acquired the lock synchronously
        }
        ops.next = waiting_;
        waiting_ = &ops;
        return true;
    }

    bool shared_mutex::enqueue_shared(ops_base& ops) noexcept
    {
        std::unique_lock lock(mut_);
        if (shared_holder_ != unique_locked && !waiting_ && !pending_)
        {
            shared_holder_++;
            return false; // Acquired the lock synchronously
        }
        ops.next = waiting_;
        waiting_ = &ops;
        return true;
    }

    // This needs to be called under the lock, but the result callbacks
    // should be called without the lock.
    ops_base* shared_mutex::get_resumption_ops() noexcept
    {
        if (!pending_)
        {
            auto* head = std::exchange(waiting_, nullptr);
            ops_base* prev = nullptr;
            while (head)
            {
                pending_ = std::exchange(head, head->next);
                pending_->next = std::exchange(prev, pending_);
            }
        }
        if (!pending_)
            return nullptr; // No one is waiting
        if (!pending_->shared) // Next one waiting is a unique lock
        {
            shared_holder_ = unique_locked;
            auto* result = std::exchange(pending_, pending_->next);
            result->next = nullptr;
            return result;
        }
        // Find a contiguous subsequence of shared lock operation states
        auto* result = pending_;
        ops_base* last;
        do
        {
            last = std::exchange(pending_, pending_->next);
            shared_holder_++;
        } while (pending_ && pending_->shared);
        last->next = nullptr;
        return result;
    }
} // namespace clu::async
