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
            ops_base* next = head->next; // *head might be destroyed after calling set() on it
            head->set();
            head = next;
        }
    }

    void manual_reset_event::reset() noexcept
    {
        // We should only set tail to nullptr if tail is currently this
        void* expected = this;
        (void)tail_.compare_exchange_strong(expected, nullptr, std::memory_order::acq_rel);
    }

    void manual_reset_event::start_ops(ops_base& ops)
    {
        void* tail = tail_.load(std::memory_order::acquire);
        while (true)
        {
            if (tail == this) // set state, complete synchronously
            {
                ops.set();
                return;
            }
            ops.next = static_cast<ops_base*>(tail);
            // try to append task to the list
            if (tail_.compare_exchange_weak(tail, &ops, //
                    std::memory_order::release, std::memory_order::acquire))
                return; // we succeeded
        }
    }
} // namespace clu::async
