#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include "execution_traits.h"

namespace clu::exec
{
    namespace detail
    {
        namespace loop
        {
            class run_loop;

            class ops_base
            {
            public:
                virtual void execute() = 0;
                ops_base* next = nullptr;
            protected:
                run_loop* loop_ = nullptr;
                explicit ops_base(run_loop* loop) noexcept: loop_(loop) {}
                ~ops_base() noexcept = default;
                void enqueue();
            };

            class snd_t;

            class schd_t
            {
            public:
                explicit schd_t(run_loop* loop) noexcept: loop_(loop) {}
                friend bool operator==(schd_t, schd_t) noexcept = default;
            private:
                run_loop* loop_;
                friend auto tag_invoke(schedule_t, schd_t schd) noexcept;
            };

            template <typename R>
            struct ops_
            {
                class type;
            };

            template <typename R>
            class ops_<R>::type final : public ops_base
            {
            public:
                template <forwarding<R> R2>
                type(run_loop* loop, R2&& recv) noexcept(std::is_nothrow_constructible_v<R, R2>):
                    ops_base(loop), recv_(static_cast<R2&&>(recv)) {}

                void execute() override
                {
                    if (exec::get_stop_token(recv_).stop_requested())
                        exec::set_stopped(std::move(recv_));
                    else
                        exec::set_value(std::move(recv_));
                }

                ~type() noexcept = default;

            private:
                R recv_;

                friend void tag_invoke(start_t, type& state) noexcept
                {
                    try { state.enqueue(); }
                    catch (...) { exec::set_error(std::move(state.recv_), std::current_exception()); }
                }
            };

            template <typename R>
            using ops_t = typename ops_<std::remove_cvref_t<R>>::type;

            class snd_t
            {
            public:
                explicit snd_t(run_loop* loop): loop_(loop) {}

            private:
                run_loop* loop_ = nullptr;

                // @formatter:off
                template <typename R>
                friend auto tag_invoke(connect_t, const snd_t& snd, R&& recv)
                noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<R>, R>)
                {
                    return ops_t<R>(snd.loop_, static_cast<R&&>(recv));
                }
                // @formatter:on

                template <recvs::completion_cpo Cpo>
                friend auto tag_invoke(get_completion_scheduler_t<Cpo>, const snd_t& snd) noexcept { return schd_t(snd.loop_); }

                friend completion_signatures<
                    set_value_t(),
                    set_error_t(std::exception_ptr),
                    set_stopped_t()
                > tag_invoke(get_completion_signatures_t, const snd_t&, auto) noexcept { return {}; }
            };

            inline auto tag_invoke(schedule_t, const schd_t schd) noexcept { return snd_t(schd.loop_); }

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
                    while (ops_base* task = dequeue(lock))
                        task->execute();
                }

                void finish()
                {
                    std::unique_lock lock(mutex_);
                    state_ = state_t::finishing;
                    cv_.notify_all();
                }

                auto get_scheduler() noexcept { return schd_t(this); }

            private:
                enum class state_t { starting, running, finishing };

                std::mutex mutex_;
                std::condition_variable cv_;

                state_t state_ = state_t::starting;
                ops_base* head_ = nullptr;
                ops_base* tail_ = nullptr;

                void enqueue(ops_base* task)
                {
                    std::unique_lock lock(mutex_);
                    tail_ = (head_ ? tail_->next : head_) = task;
                    cv_.notify_one();
                }

                ops_base* dequeue(std::unique_lock<std::mutex>& lock)
                {
                    if (!head_ && state_ == state_t::finishing)
                        return nullptr;
                    cv_.wait(lock, [this] { return head_ != nullptr; });
                    ops_base* ptr = head_;
                    head_ = head_->next;
                    if (!head_) tail_ = nullptr;
                    return ptr;
                }

                friend ops_base;
            };

            inline void ops_base::enqueue() { loop_->enqueue(this); }
        }

        namespace inl_schd
        {
            // TODO: finish this
            template <typename R>
            struct ops_
            {
                class type;
            };

            template <typename R>
            class ops_<R>::type
            {
            public:
                template <typename R2>
                explicit type(R2&& recv):
                    recv_(static_cast<R2&&>(recv)) {}

            private:
                R recv_;
            };

            template <typename R>
            using ops_t = typename ops_<std::remove_cvref_t<R>>::type;

            struct snd_t
            {
                template <typename R>
                friend ops_t<R> tag_invoke(connect_t, snd_t, R&& recv)
                {
                    return ops_t<R>(static_cast<R&&>(recv));
                }

                friend completion_signatures<
                    set_value_t(),
                    set_error_t(std::exception_ptr)
                > tag_invoke(get_completion_signatures_t, const snd_t&, auto) noexcept { return {}; }
            };

            struct inline_scheduler
            {
                constexpr friend bool operator==(inline_scheduler, inline_scheduler) noexcept { return true; }
                friend snd_t tag_invoke(schedule_t, const inline_scheduler&) noexcept { return {}; }
            };
        }
    }

    using detail::loop::run_loop;
    using detail::inl_schd::inline_scheduler;

    class single_thread_context
    {
    public:
        single_thread_context(): thr_([this] { loop_.run(); }) {}
        ~single_thread_context() noexcept = default;
        single_thread_context(const single_thread_context&) = delete;
        single_thread_context(single_thread_context&&) = delete;
        single_thread_context& operator=(const single_thread_context&) = delete;
        single_thread_context& operator=(single_thread_context&&) = delete;

        auto get_scheduler() noexcept { return loop_.get_scheduler(); }

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
