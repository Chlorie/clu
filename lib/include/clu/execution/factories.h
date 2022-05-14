#pragma once

#include "execution_traits.h"

namespace clu::exec
{
    namespace detail
    {
        namespace crt_schd
        {
            template <typename SchdSt, typename OpsSt>
            class ops_base_t
            {
            public:
                // clang-format off
                template <typename S>
                explicit ops_base_t(S&& schd_state) noexcept(std::is_nothrow_constructible_v<SchdSt, S>):
                    schd_state_(static_cast<S&&>(schd_state)) {}
                // clang-format on

                CLU_IMMOVABLE_TYPE(ops_base_t);

                OpsSt state;

                const SchdSt& scheduler_state() const noexcept { return schd_state_; }
                virtual void set() noexcept = 0;

            protected:
                ~ops_base_t() noexcept = default;

            private:
                CLU_NO_UNIQUE_ADDRESS SchdSt schd_state_;
            };

            template <typename SchdSt, typename OpsSt, typename Callback, typename Recv>
            struct ops_t_
            {
                class type;
            };

            template <typename SchdSt, typename OpsSt, typename Callback, typename Recv>
            using ops_t = typename ops_t_<SchdSt, OpsSt, Callback, std::decay_t<Recv>>::type;

            template <typename SchdSt, typename OpsSt, typename Callback, typename Recv>
            class ops_t_<SchdSt, OpsSt, Callback, Recv>::type final : public ops_base_t<SchdSt, OpsSt>
            {
            public:
                // clang-format off
            template <typename S, typename C, typename R>
            type(S&& state, C&& callback, R&& recv) noexcept(
                std::is_nothrow_constructible_v<SchdSt, S> &&
                std::is_nothrow_constructible_v<Callback, C> &&
                std::is_nothrow_constructible_v<Recv, R>):
                ops_base_t<SchdSt, OpsSt>(static_cast<S&&>(state)),
                callback_(static_cast<C&&>(callback)),
                recv_(static_cast<R&&>(recv)) {}
                // clang-format on

                void set() noexcept override
                {
                    if constexpr (unstoppable_token<stop_token_of_t<env_of_t<Recv>>>)
                        set_value(static_cast<Recv&&>(recv_)); // NOLINT(bugprone-branch-clone)
                    else if (get_stop_token(get_env(recv_)).stop_requested())
                        set_stopped(static_cast<Recv&&>(recv_));
                    else
                        set_value(static_cast<Recv&&>(recv_));
                }

            private:
                CLU_NO_UNIQUE_ADDRESS Callback callback_;
                CLU_NO_UNIQUE_ADDRESS Recv recv_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    // clang-format off
                    if constexpr (std::is_nothrow_invocable_v<Callback, ops_base_t<SchdSt, OpsSt>&>)
                        std::invoke(static_cast<Callback&>(self.callback_), self);
                    else
                        try { std::invoke(static_cast<Callback&>(self.callback_), self); }
                        catch (...) { set_error(static_cast<Recv&&>(self.recv_), std::current_exception()); }
                    // clang-format on
                }
            };

            // clang-format off
#define CLU_CREATE_SCHD_MEMBERS                                                                                        \
    public:                                                                                                            \
        template <typename S, typename C>                                                                              \
        type(S&& state, C&& callback) noexcept(                                                                        \
            std::is_nothrow_constructible_v<SchdSt, S> &&                                                              \
            std::is_nothrow_constructible_v<Callback, C>):                                                             \
            state_(static_cast<S&&>(state)),                                                                           \
            callback_(static_cast<C&&>(callback)) {}                                                                   \
                                                                                                                       \
    private:                                                                                                           \
        CLU_NO_UNIQUE_ADDRESS SchdSt state_;                                                                           \
        CLU_NO_UNIQUE_ADDRESS Callback callback_
            // clang-format on

            template <typename SchdSt, typename OpsSt, typename Callback>
            struct snd_t_
            {
                class type;
            };

            template <typename SchdSt, typename OpsSt, typename Callback>
            using snd_t = typename snd_t_<SchdSt, OpsSt, Callback>::type;

            template <typename SchdSt, typename OpsSt, typename Callback>
            struct schd_t_
            {
                class type;
            };

            template <typename SchdSt, typename OpsSt, typename Callback>
            using schd_t = typename schd_t_<std::decay_t<SchdSt>, std::decay_t<OpsSt>, std::decay_t<Callback>>::type;

            template <typename SchdSt, typename OpsSt, typename Callback>
            class snd_t_<SchdSt, OpsSt, Callback>::type
            {
                CLU_CREATE_SCHD_MEMBERS;

                friend auto tag_invoke(get_completion_scheduler_t<set_value_t>, const type& self) //
                    CLU_SINGLE_RETURN(schd_t<SchdSt, OpsSt, Callback>(self.state_, self.callback_));
                friend auto tag_invoke(get_completion_scheduler_t<set_stopped_t>, const type& self) //
                    CLU_SINGLE_RETURN(schd_t<SchdSt, OpsSt, Callback>(self.state_, self.callback_));

                template <typename Env>
                constexpr friend auto tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept
                {
                    // MSVC chokes at concepts used as constexpr booleans
                    // Also the parser chokes so much if I use filtered_sigs here...
                    constexpr bool unstoppable = requires { requires unstoppable_token<stop_token_of_t<Env>>; };
                    constexpr bool nothrow = std::is_nothrow_invocable_v<Callback, ops_base_t<SchdSt, OpsSt>&>;
                    using list = meta::remove_q<void>::fn<set_value_t(), //
                        conditional_t<unstoppable, void, set_stopped_t()>, //
                        conditional_t<nothrow, set_error_t(std::exception_ptr), void>>;
                    return meta::unpack_invoke<list, meta::quote<completion_signatures>>{};
                }

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv) //
                    CLU_SINGLE_RETURN(ops_t<SchdSt, OpsSt, Callback, R>( //
                        static_cast<SchdSt&&>(self.state_), //
                        static_cast<Callback&&>(self.callback_), //
                        static_cast<R&&>(recv)));
            };

            template <typename SchdSt, typename OpsSt, typename Callback>
            class schd_t_<SchdSt, OpsSt, Callback>::type
            {
                CLU_CREATE_SCHD_MEMBERS;

                friend bool operator==(const type& lhs, const type& rhs) //
                    CLU_SINGLE_RETURN(lhs.state_ == rhs.state_);
                friend auto tag_invoke(schedule_t, const type& self) //
                    CLU_SINGLE_RETURN(snd_t<SchdSt, OpsSt, Callback>(self.state_, self.callback_));
            };

#undef CLU_CREATE_SCHD_MEMBERS
        } // namespace crt_schd

        namespace crt_snd
        {
            template <typename Fn, typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename Fn, typename R>
            using ops_t = typename ops_t_<Fn, std::decay_t<R>>::type;

            template <typename Fn, typename R>
            class ops_t_<Fn, R>::type
            {
            public:
                // clang-format off
                template <typename Fn2, typename R2>
                type(Fn2&& func, R2&& recv) noexcept(
                    std::is_nothrow_constructible_v<Fn, Fn2> &&
                    std::is_nothrow_constructible_v<R, R2>):
                    func_(static_cast<Fn2&&>(func)),
                    recv_(static_cast<R2&&>(recv)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS Fn func_;
                CLU_NO_UNIQUE_ADDRESS R recv_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    // clang-format off
                    if constexpr (std::is_nothrow_invocable_v<Fn, R>)
                        std::invoke(static_cast<Fn&&>(self.func_), static_cast<R&&>(self.recv_));
                    else
                        try { std::invoke(static_cast<Fn&&>(self.func_), static_cast<R&&>(self.recv_)); }
                        catch (...) { set_error(static_cast<R&&>(self.recv_), std::current_exception()); }
                    // clang-format on
                }
            };

            template <typename Fn, typename Sigs>
            struct snd_t_
            {
                class type;
            };

            template <typename Fn, typename Sigs>
            using snd_t = typename snd_t_<std::decay_t<Fn>, Sigs>::type;

            template <typename Fn, typename Sigs>
            class snd_t_<Fn, Sigs>::type
            {
            public:
                // clang-format off
                template <typename Fn2>
                explicit type(Fn2&& func) noexcept(std::is_nothrow_constructible_v<Fn, Fn2>):
                    func_(static_cast<Fn2&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS Fn func_;

                template <typename R>
                friend void tag_invoke(connect_t, type&& self, R&& recv) //
                    CLU_SINGLE_RETURN(ops_t<Fn, R>(static_cast<Fn&&>(self.func_), static_cast<R&&>(recv)));

                // clang-format off
                constexpr friend Sigs tag_invoke( //
                    get_completion_signatures_t, const type&, auto&&) noexcept { return {}; }
                // clang-format on
            };
        } // namespace crt_snd
    } // namespace detail

    template <typename OpsSt, typename SchdSt>
    using scheduler_operation_base = detail::crt_schd::ops_base_t<SchdSt, OpsSt>;

    template <typename OpsSt, std::equality_comparable SchdSt,
        std::invocable<scheduler_operation_base<OpsSt, SchdSt>&> Callback>
    auto create_scheduler(SchdSt&& schd_state, Callback&& callback)
        CLU_SINGLE_RETURN(detail::crt_schd::schd_t<SchdSt, OpsSt, Callback>(
            static_cast<SchdSt&&>(schd_state), static_cast<Callback&&>(callback)));

    template <template_of<completion_signatures> Sigs, typename Fn>
    auto create_sender(Fn&& func) CLU_SINGLE_RETURN(detail::crt_snd::snd_t<Fn, Sigs>(static_cast<Fn&&>(func)));
} // namespace clu::exec
