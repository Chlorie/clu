#include "clu/stop_token.h"
#include "clu/assertion.h"

namespace clu
{
    namespace detail
    {
        void in_place_stop_cb_base::attach() noexcept
        {
            if (src_ && !src_->try_attach(this))
            {
                // Already locked, stop requested
                src_ = nullptr;
                execute();
            }
        }

        void in_place_stop_cb_base::detach() noexcept
        {
            if (src_)
                src_->detach(this);
        }
    } // namespace detail

    in_place_stop_source::~in_place_stop_source() noexcept
    {
        CLU_ASSERT(callbacks_.unsafe_load_relaxed() == nullptr,
            "in_place_stop_source destroyed before the callbacks are done");
    }

    in_place_stop_token in_place_stop_source::get_token() noexcept { return in_place_stop_token(this); }

    bool in_place_stop_source::request_stop() noexcept
    {
        auto [locked, current] = lock_if_not_requested();
        if (!locked)
            return false;
        requesting_thread_ = std::this_thread::get_id();
        requested_.store(true, std::memory_order::release);
        while (current)
        {
            // Detach the first callback
            auto* new_head = current->next_;
            if (new_head)
                new_head->prev_ = nullptr;
            current->state_.store(started, std::memory_order::release);
            callbacks_.store_and_unlock(new_head);
            // Now that we have released the lock, start executing the callback
            current->execute();
            if (!current->removed_during_exec_) // detach() not called during execute()
            {
                // Change the state and notify
                current->state_.store(completed, std::memory_order::release);
                current->state_.notify_one();
            }
            current = callbacks_.lock_and_load();
        }
        callbacks_.store_and_unlock(nullptr);
        return true;
    }

    bool in_place_stop_source::try_attach(detail::in_place_stop_cb_base* cb) noexcept
    {
        const auto [locked, head] = lock_if_not_requested();
        if (!locked)
            return false;
        // Add the new one before the current head
        cb->next_ = head;
        if (head)
            head->prev_ = cb;
        callbacks_.store_and_unlock(cb);
        return true;
    }

    void in_place_stop_source::detach(detail::in_place_stop_cb_base* cb) noexcept
    {
        auto* head = callbacks_.lock_and_load();
        if (cb->state_.load(std::memory_order::acquire) == not_started)
        {
            // Hasn't started executing
            if (cb->next_)
                cb->next_->prev_ = cb->prev_;
            if (cb->prev_) // In list
            {
                cb->prev_->next_ = cb->next_;
                callbacks_.store_and_unlock(head);
            }
            else // Is head of list
                callbacks_.store_and_unlock(cb->next_);
        }
        else
        {
            // Already started executing
            const auto id = requesting_thread_;
            callbacks_.store_and_unlock(head); // Just unlock, we won't modify the linked list
            if (std::this_thread::get_id() == id) // execute() called detach()
                cb->removed_during_exec_ = true;
            else // Executing on a different thread
                cb->state_.wait(completed, std::memory_order::acquire); // Wait until the callback completes
        }
    }

    in_place_stop_source::lock_result in_place_stop_source::lock_if_not_requested() noexcept
    {
        if (stop_requested())
            return {}; // Fast path
        auto* head = callbacks_.lock_and_load();
        // Check again in case someone changed the state
        // while we're trying to acquire the lock
        if (stop_requested())
        {
            callbacks_.store_and_unlock(head);
            return {};
        }
        return {true, head};
    }
} // namespace clu
