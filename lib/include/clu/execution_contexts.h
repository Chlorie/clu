#pragma once

#include <thread>

#include "execution.h"

namespace clu
{
    namespace detail::inl_schd
    {
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
            explicit type(R2&& recv): recv_(static_cast<R2&&>(recv))
            {
            }

            CLU_IMMOVABLE_TYPE(type);

        private:
            R recv_;

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                exec::set_value(static_cast<R&&>(self.recv_));
            }
        };

        template <typename R>
        using ops_t = typename ops_<std::remove_cvref_t<R>>::type;

        struct snd_t
        {
            using completion_signatures = exec::completion_signatures<exec::set_value_t()>;

            template <typename R>
            friend ops_t<R> tag_invoke(exec::connect_t, snd_t, R&& recv)
            {
                return ops_t<R>(static_cast<R&&>(recv));
            }
        };

        struct inline_scheduler
        {
            constexpr friend bool operator==(inline_scheduler, inline_scheduler) noexcept { return true; }
            friend snd_t tag_invoke(exec::schedule_t, const inline_scheduler&) noexcept { return {}; }
        };
    } // namespace detail::inl_schd

    using detail::inl_schd::inline_scheduler;

    class single_thread_context
    {
    public:
        single_thread_context(): thr_([this] { loop_.run(); }) {}
        ~single_thread_context() noexcept { finish(); }
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

        std::thread::id get_id() const noexcept { return thr_.get_id(); }

    private:
        run_loop loop_;
        std::thread thr_;
    };
} // namespace clu
