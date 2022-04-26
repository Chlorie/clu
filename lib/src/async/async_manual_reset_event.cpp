#include "clu/async/async_manual_reset_event.h"

namespace clu
{
    void async_manual_reset_event::set() noexcept
    {
        void* tail = tail_.exchange(this, std::memory_order::acq_rel);
        if (tail == this || tail == nullptr)
            return; // already set by some other thread, or the queue is straight up empty, just return
        // or else we've got the queue (intrusive linked list), though in the reverse order
        // so we reverse the linked list here
        auto* head = static_cast<ops_base*>(tail);
        ops_base* prev = nullptr;
        while (head)
        {
            auto* next = head->next;
            head->next = prev;
            prev = head;
            head = next;
        }
        head = prev;
        // list reversed, now just traverse the list and execute the tasks one by one
        while (head)
        {
            head->execute();
            head = head->next;
        }
    }

    void async_manual_reset_event::reset() noexcept
    {
        // We should only set tail to nullptr if tail is currently this
        void* expected = this;
        (void)tail_.compare_exchange_strong(expected, nullptr, std::memory_order::acq_rel);
    }

    // The enqueue customization point
    void tag_invoke(exec::add_operation_t, //
        async_manual_reset_event& self, async_manual_reset_event::ops_base& task)
    {
        using ops_base = async_manual_reset_event::ops_base;
        void* tail = self.tail_.load(std::memory_order::acquire);
        while (true)
        {
            if (tail == &self) // set state, complete synchronously
            {
                task.execute();
                return;
            }
            task.next = static_cast<ops_base*>(tail);
            // try to append task to the list
            if (self.tail_.compare_exchange_weak(tail, &task, //
                    std::memory_order::release, std::memory_order::acquire))
                return; // we succeeded
        }
    }
} // namespace clu
