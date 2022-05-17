#pragma once

#include <thread>

#include "timed_threads.h"
#include "../execution/execution_traits.h"
#include "../execution/utility.h"
#include "../scope.h"

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
    class timer_loop;

    CLU_SUPPRESS_EXPORT_WARNING
    namespace detail::tm_loop
    {
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;

        enum class rbt_color : bool
        {
            black = false,
            red = true
        };

        // Maintains an intrusive red-black tree
        class CLU_API ops_base
        {
        public:
            time_point deadline{};
            std::uintptr_t parent_and_color{}; // Last bit is color
            ops_base* children[2]{};
            bool cancelled = false;

            ops_base() noexcept = default;
            CLU_IMMOVABLE_TYPE(ops_base);

            virtual void set() noexcept = 0;

            ops_base* parent() const noexcept
            {
                // NOLINTNEXTLINE(performance-no-int-to-ptr)
                return reinterpret_cast<ops_base*>(parent_and_color & mask);
            }
            rbt_color color() const noexcept { return static_cast<rbt_color>(parent_and_color & 1); }

            void set_parent_and_color(ops_base* parent, const rbt_color col) noexcept
            {
                parent_and_color = reinterpret_cast<std::uintptr_t>(parent) | static_cast<std::uintptr_t>(col);
            }
            void recolor(const rbt_color col) noexcept
            {
                parent_and_color = (parent_and_color & mask) | static_cast<std::uintptr_t>(col);
            }
            void reparent(ops_base* parent) noexcept
            {
                parent_and_color = reinterpret_cast<std::uintptr_t>(parent) | (parent_and_color & 1);
            }

            // The child pointer to this in the parent of this
            ops_base*& this_from_parent() const noexcept
            {
                ops_base* p = parent();
                return (this == p->children[0]) ? p->children[0] : p->children[1];
            }

        protected:
            ~ops_base() noexcept = default;

        private:
            static constexpr std::uintptr_t mask = ~static_cast<std::uintptr_t>(1);
        };
        static_assert(alignof(ops_base) >= 2);

        class ops_rbtree
        {
        public:
            void insert(ops_base* node) noexcept;
            void remove(ops_base* node) noexcept;
            bool empty() const noexcept { return !root_; }
            ops_base* minimum() const noexcept { return minimum_; }

        private:
            static constexpr std::size_t left = 0;
            static constexpr std::size_t right = 1;
            static constexpr std::size_t other(const std::size_t direction) noexcept { return 1 - direction; }

            ops_base* root_ = nullptr;
            ops_base* minimum_ = nullptr;

            ops_base* rotate(ops_base* node, std::size_t dir) noexcept;
            std::pair<ops_base*, std::size_t> find_insert_position(const ops_base* node) const noexcept;
            void update_minimum() noexcept;
            bool remove_simple_cases(ops_base* node) noexcept;
            void swap_with_successor(ops_base* node) noexcept;

            // Which child is the current node of its parent node
            static std::size_t which_child(const ops_base* node) noexcept
            {
                return node->parent()->children[left] == node ? left : right;
            }

            static void fix_parent_of_child(ops_base* node, const std::size_t direction) noexcept
            {
                if (ops_base* child = node->children[direction])
                    child->reparent(node);
            }
        };

        template <typename R>
        class ops_recv_base : public ops_base
        {
        public:
            // clang-format off
            template <forwarding<R> R2>
            explicit ops_recv_base(timer_loop* loop, R2&& recv):
                loop_(loop), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set() noexcept override
            {
                cb_.reset();
                if (this->cancelled || exec::get_stop_token(exec::get_env(recv_)).stop_requested())
                    exec::set_stopped(static_cast<R&&>(recv_));
                else
                    exec::set_value(static_cast<R&&>(recv_));
            }

        protected:
            ~ops_recv_base() noexcept = default;

            void start() noexcept
            {
                const auto token = exec::get_stop_token(exec::get_env(recv_));
                if (token.stop_requested())
                {
                    exec::set_stopped(static_cast<R&&>(recv_));
                    return;
                }
                try
                {
                    cb_.emplace(token, callback{this});
                    scope_fail guard{[&] { cb_.reset(); }};
                    start_ops(*loop_, *this);
                }
                catch (...)
                {
                    exec::set_error(static_cast<R&&>(recv_), std::current_exception());
                }
            }

        private:
            struct callback
            {
                ops_recv_base* self;
                void operator()() noexcept { stop_ops(*self->loop_, *self); }
            };
            using stop_callback_t = typename exec::stop_token_of_t<exec::env_of_t<R>>::template callback_type<callback>;

            timer_loop* loop_;
            CLU_NO_UNIQUE_ADDRESS R recv_;
            std::optional<stop_callback_t> cb_;
        };

        template <typename R>
        struct at_ops_t_
        {
            class type;
        };

        template <typename R>
        using at_ops_t = typename at_ops_t_<std::decay_t<R>>::type;

        template <typename R>
        class at_ops_t_<R>::type final : public ops_recv_base<R>
        {
        public:
            // clang-format off
            template <typename R2>
            type(timer_loop* loop, R2&& recv, const time_point tp):
                ops_recv_base(loop, static_cast<R2&&>(recv)) { this->deadline = tp; }
            // clang-format on

        private:
            friend void tag_invoke(exec::start_t, type& self) noexcept { self.start(); }
        };

        template <typename R, typename Dur>
        struct after_ops_t_
        {
            class type;
        };

        template <typename R, typename Dur>
        using after_ops_t = typename after_ops_t_<std::decay_t<R>, Dur>::type;

        template <typename R, typename Dur>
        class after_ops_t_<R, Dur>::type final : public ops_recv_base<R>
        {
        public:
            // clang-format off
            template <typename R2>
            type(timer_loop* loop, R2&& recv, const Dur dur):
                ops_recv_base<R>(loop, static_cast<R2&&>(recv)), dur_(dur) {}
            // clang-format on

        private:
            Dur dur_;

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                self.deadline = clock::now() + self.dur_; // Deadline decided at start time
                self.start();
            }
        };

        class schd_t;

        using sigs = exec::completion_signatures< //
            exec::set_value_t(), exec::set_error_t(std::exception_ptr), exec::set_stopped_t()>;

        template <typename Dur>
        struct after_snd_t_
        {
            class type;
        };

        template <typename Dur>
        using after_snd_t = typename after_snd_t_<Dur>::type;

        template <typename Dur>
        class after_snd_t_<Dur>::type
        {
        public:
            using completion_signatures = sigs;
            explicit type(timer_loop* loop, const Dur dur) noexcept: loop_(loop), dur_(dur) {}

        private:
            timer_loop* loop_;
            Dur dur_;

            template <same_as_any_of<exec::set_value_t, exec::set_stopped_t> Cpo>
            friend auto tag_invoke(exec::get_completion_scheduler_t<Cpo>, const type& self) noexcept
            {
                // ReSharper disable once CppClassIsIncomplete
                return schd_t(self.loop_);
            }

            template <typename R>
            friend auto tag_invoke(exec::connect_t, type&& self, R&& recv)
            {
                return after_ops_t<R, Dur>(self.loop_, static_cast<R&&>(recv), self.dur_);
            }
        };

        template <typename Dur>
        auto make_after_snd(timer_loop* loop, const Dur dur) noexcept
        {
            return after_snd_t<Dur>(loop, dur);
        }

        class CLU_API schd_t
        {
        public:
            explicit schd_t(timer_loop* loop) noexcept: loop_(loop) {}

        private:
            class CLU_API at_snd_t
            {
            public:
                using completion_signatures = sigs;
                explicit at_snd_t(timer_loop* loop, const time_point tp) noexcept: loop_(loop), tp_(tp) {}

            private:
                timer_loop* loop_;
                time_point tp_;

                template <same_as_any_of<exec::set_value_t, exec::set_stopped_t> Cpo>
                friend auto tag_invoke(exec::get_completion_scheduler_t<Cpo>, const at_snd_t& self) noexcept
                {
                    return schd_t(self.loop_);
                }

                template <typename R>
                friend auto tag_invoke(exec::connect_t, at_snd_t&& self, R&& recv)
                {
                    return at_ops_t<R>(self.loop_, static_cast<R&&>(recv), self.tp_);
                }
            };

            timer_loop* loop_ = nullptr;

            friend bool operator==(schd_t, schd_t) noexcept = default;

            friend auto tag_invoke(exec::schedule_t, const schd_t self) noexcept
            {
                return make_after_snd(self.loop_, std::chrono::seconds{0});
            }
            friend auto tag_invoke(exec::schedule_at_t, const schd_t self, const time_point tp) noexcept
            {
                return at_snd_t(self.loop_, tp);
            }
            friend auto tag_invoke(exec::schedule_after_t, const schd_t self, const exec::duration auto dur) noexcept
            {
                return tm_loop::make_after_snd(self.loop_, dur);
            }
        };
    } // namespace detail::tm_loop

    class CLU_API timer_loop
    {
    public:
        using clock = std::chrono::steady_clock;

        timer_loop() noexcept = default;
        ~timer_loop() noexcept;
        CLU_IMMOVABLE_TYPE(timer_loop);

        void run();
        void finish();
        auto get_scheduler() noexcept { return detail::tm_loop::schd_t(this); }

    private:
        using ops_base = detail::tm_loop::ops_base;

        std::mutex mutex_;
        std::condition_variable cv_;

        bool stopped_ = false;
        detail::tm_loop::ops_rbtree tree_;

        friend void start_ops(timer_loop& self, ops_base& ops) { self.enqueue(ops); }
        friend void stop_ops(timer_loop& self, ops_base& ops) noexcept { self.cancel(ops); }
        void enqueue(ops_base& ops);
        void cancel(ops_base& ops) noexcept;
        ops_base* dequeue();
    };
    static_assert(alignof(timer_loop) >= 2);

    class CLU_API timer_thread_context
    {
    public:
        timer_thread_context(): thr_([this] { loop_.run(); }) {}
        ~timer_thread_context() noexcept { finish(); }
        timer_thread_context(const timer_thread_context&) = delete;
        timer_thread_context(timer_thread_context&&) = delete;
        timer_thread_context& operator=(const timer_thread_context&) = delete;
        timer_thread_context& operator=(timer_thread_context&&) = delete;

        auto get_scheduler() noexcept { return loop_.get_scheduler(); }

        void finish()
        {
            loop_.finish();
            if (thr_.joinable())
                thr_.join();
        }

        std::thread::id get_id() const noexcept { return thr_.get_id(); }

    private:
        timer_loop loop_;
        std::thread thr_;
    };
    CLU_RESTORE_EXPORT_WARNING
} // namespace clu
