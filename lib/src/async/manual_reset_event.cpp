#include "clu/async/manual_reset_event.h"

namespace clu::async
{
    using detail::amre::ops_base;

    void manual_reset_event::set() noexcept
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
            head->set();
            head = head->next;
        }
    }

    void manual_reset_event::reset() noexcept
    {
        // We should only set tail to nullptr if tail is currently this
        void* expected = this;
        (void)tail_.compare_exchange_strong(expected, nullptr, std::memory_order::acq_rel);
    }

    void start_ops(manual_reset_event& self, ops_base& ops)
    {
        void* tail = self.tail_.load(std::memory_order::acquire);
        while (true)
        {
            if (tail == &self) // set state, complete synchronously
            {
                ops.set();
                return;
            }
            ops.next = static_cast<ops_base*>(tail);
            // try to append task to the list
            if (self.tail_.compare_exchange_weak(tail, &ops, //
                    std::memory_order::release, std::memory_order::acquire))
                return; // we succeeded
        }
    }
} // namespace clu::async
