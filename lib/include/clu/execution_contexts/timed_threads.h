#pragma once

#include <thread>

#include "../execution/execution_traits.h"
#include "../execution/utility.h"

namespace clu
{
    namespace detail::new_thrd_ctx
    {
        using clock = std::chrono::steady_clock;

        template <typename Env>
        using sigs = exec::detail::filtered_sigs<exec::set_value_t(), exec::set_error_t(std::exception_ptr),
            conditional_t<exec::detail::stoppable_env<Env>, exec::set_stopped_t(), void>>;

        // exec::schedule

        template <typename R>
        struct ops_t_
        {
            class type;
        };

        template <typename R>
        using ops_t = typename ops_t_<std::decay_t<R>>::type;

        template <typename R>
        class ops_t_<R>::type
        {
        public:
            // clang-format off
            template <forwarding<R> R2>
            explicit type(R2&& recv) noexcept(std::is_nothrow_constructible_v<R, R2>):
                recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

        private:
            CLU_NO_UNIQUE_ADDRESS R recv_;

            void start() noexcept
            {
                if constexpr (unstoppable_token<exec::stop_token_of_t<exec::env_of_t<R>>>)
                    exec::set_value(static_cast<R&&>(recv_)); // NOLINT(bugprone-branch-clone)
                else if (exec::get_stop_token(exec::get_env(recv_)).stop_requested())
                    exec::set_stopped(static_cast<R&&>(recv_));
                else
                    exec::set_value(static_cast<R&&>(recv_));
            }

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                // clang-format off
                try { std::thread([&] { self.start(); }).detach(); }
                catch (...) { exec::set_error(static_cast<R&&>(self.recv_), std::current_exception()); }
                // clang-format on
            }
        };

        struct schd_t;

        struct snd_t
        {
            // clang-format off
            template <typename Env>
            constexpr friend sigs<Env> tag_invoke(
                exec::get_completion_signatures_t, snd_t, Env&&) noexcept { return {}; }

            template <same_as_any_of<exec::set_value_t, exec::set_stopped_t> Cpo>
            constexpr friend auto tag_invoke(
                exec::get_completion_scheduler_t<Cpo>, snd_t) noexcept { return schd_t{}; }
            // clang-format on

            template <typename R>
            friend auto tag_invoke(exec::connect_t, snd_t, R&& recv) //
                CLU_SINGLE_RETURN(ops_t<R>(static_cast<R&&>(recv)));
        };

        // exec::schedule_at / exec::schedule_after

        template <typename Fn, typename R>
        struct timed_non_stop_ops_t_
        {
            class type;
        };
        template <typename Fn, typename R>
        class timed_non_stop_ops_t_<Fn, R>::type
        {
        public:
            // clang-format off
            template <typename R2>
            type(Fn fn, R2&& recv) noexcept(std::is_nothrow_constructible_v<R, R2>):
                fn_(fn), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

        private:
            Fn fn_;
            R recv_;

            void start() noexcept
            {
                std::this_thread::sleep_until(fn_());
                exec::set_value(static_cast<R&&>(recv_)); // NOLINT(bugprone-branch-clone)
            }

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                // clang-format off
                try { std::thread([&] { self.start(); }).detach(); }
                catch (...) { exec::set_error(static_cast<R&&>(self.recv_), std::current_exception()); }
                // clang-format on
            }
        };

        template <typename Fn, typename R>
        struct timed_stoppable_ops_t_
        {
            class type;
        };

        template <typename Fn, typename R>
        class timed_stoppable_ops_t_<Fn, R>::type
        {
        public:
            // clang-format off
            template <typename R2>
            type(Fn fn, R2&& recv): fn_(fn), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

        private:
            struct stop_callback
            {
                type& self;
                void operator()() const noexcept { self.cv_.notify_one(); }
            };

            using stop_token_t = exec::stop_token_of_t<exec::env_of_t<R>>;
            using callback_t = typename stop_token_t::template callback_type<stop_callback>;

            Fn fn_;
            CLU_NO_UNIQUE_ADDRESS R recv_;
            std::mutex mut_;
            std::condition_variable cv_;
            std::optional<callback_t> callback_;

            void work(const stop_token_t& token) noexcept
            {
                {
                    std::unique_lock lck(mut_);
                    cv_.wait_until(lck, fn_(), [=] { return token.stop_requested(); });
                }
                callback_.reset();
                if (token.stop_requested())
                    exec::set_stopped(static_cast<R&&>(recv_));
                else
                    exec::set_value(static_cast<R&&>(recv_));
            }

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                const auto token = exec::get_stop_token(exec::get_env(self.recv_));
                self.callback_.emplace(token, stop_callback{self});
                if (token.stop_requested())
                    exec::set_stopped(static_cast<R&&>(self.recv_));
                std::thread([&self, token] { self.work(token); }).detach();
            }
        };

        template <typename Fn, typename R>
        using timed_ops_t = typename conditional_t<unstoppable_token<exec::stop_token_of_t<exec::env_of_t<R>>>,
            timed_non_stop_ops_t_<Fn, std::decay_t<R>>, timed_stoppable_ops_t_<Fn, std::decay_t<R>>>::type;

        template <typename Fn>
        struct timed_snd_t_
        {
            class type;
        };

        template <typename Fn>
        using timed_snd_t = typename timed_snd_t_<std::decay_t<Fn>>::type;

        template <typename Fn>
        class timed_snd_t_<Fn>::type
        {
        public:
            explicit type(Fn fn) noexcept: fn_(fn) {}

        private:
            Fn fn_;

            // clang-format off
            template <typename Env>
            constexpr friend sigs<Env> tag_invoke(
                exec::get_completion_signatures_t, type, Env&&) noexcept { return {}; }

            template <same_as_any_of<exec::set_value_t, exec::set_stopped_t> Cpo>
            constexpr friend auto tag_invoke(
                exec::get_completion_scheduler_t<Cpo>, type) noexcept { return schd_t{}; }
            // clang-format on

            template <typename R>
            friend auto tag_invoke(exec::connect_t, const type self, R&& recv) //
                CLU_SINGLE_RETURN(timed_ops_t<Fn, R>(self.fn_, static_cast<R&&>(recv)));
        };

        template <typename Fn>
        auto make_timed_snd(Fn fn) noexcept
        {
            return timed_snd_t<Fn>(fn);
        }

        // Scheduler type

        struct schd_t
        {
            constexpr friend bool operator==(schd_t, schd_t) noexcept { return true; }
            constexpr friend snd_t tag_invoke(exec::schedule_t, schd_t) noexcept { return {}; }
            friend auto tag_invoke(exec::now_t, schd_t) noexcept { return clock::now(); }

            friend auto tag_invoke(exec::schedule_at_t, schd_t, const clock::time_point tp) noexcept
            {
                return make_timed_snd([=]() noexcept { return tp; });
            }

            friend auto tag_invoke(exec::schedule_after_t, schd_t, const exec::duration auto dur) noexcept
            {
                return new_thrd_ctx::make_timed_snd([=]() noexcept { return clock::now() + dur; });
            }
        };
    } // namespace detail::new_thrd_ctx

    using new_thread_scheduler = detail::new_thrd_ctx::schd_t;
} // namespace clu
