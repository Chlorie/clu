#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include "execution_traits.h"

namespace clu::exec
{
    class run_loop
    {
    public:
        run_loop() noexcept = default;
        run_loop(const run_loop&) = delete;
        run_loop(run_loop&&) = delete;
        run_loop& operator=(const run_loop&) = delete;
        run_loop& operator=(run_loop&&) = delete;

        ~run_loop() noexcept
        {
            if (head_ || state_ == state_t::running)
                std::terminate();
        }

        void run()
        {
            std::unique_lock lock(mutex_);
            while (op_state_base* task = dequeue(lock))
                task->execute();
        }

        void finish()
        {
            std::unique_lock lock(mutex_);
            state_ = state_t::finishing;
            cv_.notify_all();
        }

    // private:
        class op_state_base
        {
        public:
            virtual void execute() = 0;
            op_state_base* next = nullptr;
        protected:
            run_loop* loop_ = nullptr;
            explicit op_state_base(run_loop* loop) noexcept: loop_(loop) {}
            ~op_state_base() noexcept = default;
        };

        class snd_t;

        class schd_t
        {
        public:
            explicit schd_t(run_loop* loop) noexcept: loop_(loop) {}
            friend bool operator==(schd_t, schd_t) noexcept = default;
        private:
            using snd_t_ = snd_t;
            run_loop* loop_;
            friend auto tag_invoke(schedule_t, const schd_t schd) noexcept { return snd_t_(schd.loop_); }
        };

        class snd_t : public completion_signatures<
                set_value_t(),
                set_error_t(std::exception_ptr),
                set_stopped_t()>
        {
        public:
            explicit snd_t(run_loop* loop): loop_(loop) {}

        private:
            template <receiver_of R>
            class op_state_t final : public op_state_base
            {
            public:
                template <forwarding<R> R2>
                op_state_t(run_loop* loop, R2&& recv) noexcept(std::is_nothrow_constructible_v<R, R2>):
                    op_state_base(loop), recv_(static_cast<R2&&>(recv)) {}

                void execute() override
                {
                    if (exec::get_stop_token(recv_).stop_requested())
                        exec::set_stopped(std::move(recv_));
                    else
                        exec::try_set_value(std::move(recv_));
                }

                ~op_state_t() noexcept = default;

            private:
                R recv_;

                void enqueue() { loop_->enqueue(this); }

                friend void tag_invoke(start_t, op_state_t& state) noexcept
                {
                    try { state.enqueue(); }
                    catch (...) { exec::set_error(std::move(state.recv_), std::current_exception()); }
                }
            };

            run_loop* loop_ = nullptr;

            // @formatter:off
            template <receiver_of R>
            friend auto tag_invoke(connect_t, const snd_t& snd, R&& recv)
                noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<R>, R>)
            {
                return op_state_t<std::remove_cvref_t<R>>(
                    snd.loop_, static_cast<R&&>(recv));
            }
            // @formatter:on

            using schd_t_ = schd_t;

            template <detail::completion_cpo Cpo>
            friend auto tag_invoke(get_completion_scheduler_t<Cpo>, const snd_t& snd) noexcept { return schd_t_(snd.loop_); }

            friend completion_signatures<set_value_t(), set_stopped_t()> tag_invoke(get_completion_signatures_t, const snd_t&);
        };

        enum class state_t { starting, running, finishing };

    public:
        scheduler auto get_scheduler() noexcept { return schd_t(this); }

    private:
        std::mutex mutex_;
        std::condition_variable cv_;

        state_t state_ = state_t::starting;
        op_state_base* head_ = nullptr;
        op_state_base* tail_ = nullptr;

        void enqueue(op_state_base* task)
        {
            std::unique_lock lock(mutex_);
            tail_ = (head_ ? tail_->next : head_) = task;
            cv_.notify_one();
        }

        op_state_base* dequeue(std::unique_lock<std::mutex>& lock)
        {
            if (!head_ && state_ == state_t::finishing)
                return nullptr;
            cv_.wait(lock, [this] { return head_ != nullptr; });
            op_state_base* ptr = head_;
            head_ = head_->next;
            if (!head_) tail_ = nullptr;
            return ptr;
        }
    };

    class single_thread_context
    {
    public:
        single_thread_context(): thr_([this] { loop_.run(); }) {}
        ~single_thread_context() noexcept = default;
        single_thread_context(const single_thread_context&) = delete;
        single_thread_context(single_thread_context&&) = delete;
        single_thread_context& operator=(const single_thread_context&) = delete;
        single_thread_context& operator=(single_thread_context&&) = delete;

        scheduler auto get_scheduler() noexcept { return loop_.get_scheduler(); }

        void finish()
        {
            loop_.finish();
            if (thr_.joinable())
                thr_.join();
        }

    private:
        run_loop loop_;
        std::thread thr_;
    };
}
