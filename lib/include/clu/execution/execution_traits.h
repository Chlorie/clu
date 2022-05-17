#pragma once

#include <variant>

#include "awaitable_traits.h"
#include "../assertion.h"
#include "../stop_token.h"
#include "../tag_invoke.h"
#include "../meta_list.h"
#include "../meta_algorithm.h"
#include "../unique_coroutine_handle.h"
#include "../piper.h"

namespace clu::exec
{
    namespace detail::exec_envs
    {
        struct no_env
        {
            friend void tag_invoke(auto, std::same_as<no_env> auto, auto&&...) = delete;
        };

        struct empty_env
        {
        };

        struct get_env_t
        {
            template <typename R>
                requires tag_invocable<get_env_t, R>
            decltype(auto) operator()(R&& recv) const noexcept
            {
                static_assert(nothrow_tag_invocable<get_env_t, R>, "get_env should be noexcept");
                return tag_invoke(*this, static_cast<R&&>(recv));
            }
        };

        struct forwarding_env_query_t
        {
            template <typename Cpo>
            constexpr bool operator()(const Cpo cpo) const noexcept
            {
                if constexpr (tag_invocable<forwarding_env_query_t, Cpo>)
                {
                    static_assert(
                        nothrow_tag_invocable<forwarding_env_query_t, Cpo>, "forwarding_env_query should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<forwarding_env_query_t, Cpo>>);
                    return tag_invoke(*this, cpo) ? true : false;
                }
                else
                    return true;
            }
        };

        // clang-format off
        template <typename T>
        concept fwd_env_query = forwarding_env_query_t{}(T{});
        // clang-format on
    } // namespace detail::exec_envs

    using detail::exec_envs::no_env;
    using detail::exec_envs::empty_env; // Not to spec
    using detail::exec_envs::get_env_t;
    using detail::exec_envs::forwarding_env_query_t;
    inline constexpr get_env_t get_env{};
    inline constexpr forwarding_env_query_t forwarding_env_query{};

    // clang-format off
    template <typename T>
    concept environment_provider =
        requires(T& prov) { { get_env(std::as_const(prov)) } -> same_as_none_of<no_env, void>; };
    // clang-format on

    template <typename T>
    using env_of_t = call_result_t<get_env_t, T>;

    namespace detail::recvs
    {
        struct set_value_t
        {
            template <typename R, typename... Vals>
                requires tag_invocable<set_value_t, R, Vals...>
            void operator()(R&& recv, Vals&&... vals) const noexcept
            {
                static_assert(nothrow_tag_invocable<set_value_t, R, Vals...>, "set_value should be noexcept");
                tag_invoke(*this, static_cast<R&&>(recv), static_cast<Vals&&>(vals)...);
            }
        };

        struct set_error_t
        {
            template <typename R, typename Err>
                requires tag_invocable<set_error_t, R, Err>
            void operator()(R&& recv, Err&& err) const noexcept
            {
                static_assert(nothrow_tag_invocable<set_error_t, R, Err>, "set_error should be noexcept");
                tag_invoke(*this, static_cast<R&&>(recv), static_cast<Err&&>(err));
            }
        };

        struct set_stopped_t
        {
            template <typename R>
                requires tag_invocable<set_stopped_t, R>
            void operator()(R&& recv) const noexcept
            {
                static_assert(nothrow_tag_invocable<set_stopped_t, R>, "set_stopped should be noexcept");
                tag_invoke(*this, static_cast<R&&>(recv));
            }
        };

        template <typename Cpo>
        concept completion_cpo = same_as_any_of<Cpo, set_value_t, set_error_t, set_stopped_t>;
    } // namespace detail::recvs

    using detail::recvs::set_value_t;
    using detail::recvs::set_error_t;
    using detail::recvs::set_stopped_t;
    inline constexpr set_value_t set_value{};
    inline constexpr set_error_t set_error{};
    inline constexpr set_stopped_t set_stopped{};

    namespace detail
    {
        template <std::same_as<set_value_t> Cpo, template <typename...> typename Tuple = type_list, typename... Args>
        type_tag_t<Tuple<Args...>> comp_sig_impl(Cpo (*)(Args...));
        template <std::same_as<set_error_t> Cpo, typename Err>
        Err comp_sig_impl(Cpo (*)(Err));
        template <std::same_as<set_stopped_t> Cpo>
        bool comp_sig_impl(Cpo (*)());
        template <typename T = void, template <typename...> typename Tuple = type_list>
        void comp_sig_impl(...);

        template <typename Fn>
        concept completion_signature =
            (!std::same_as<decltype(detail::comp_sig_impl(static_cast<Fn*>(nullptr))), void>);
    } // namespace detail

    // clang-format off
    template <detail::completion_signature... Fns> struct completion_signatures {};
    template <typename E> struct dependent_completion_signatures {};
    // clang-format on

    namespace detail
    {
        template <template <typename...> typename Tuple, template <typename...> typename Variant, typename... Fns>
        auto value_types_impl(completion_signatures<Fns...>) //
            -> meta::unpack_invoke< //
                meta::transform_l< //
                    meta::remove_q<void>::fn< //
                        decltype(detail::comp_sig_impl<set_value_t, Tuple>(static_cast<Fns*>(nullptr)))...>,
                    meta::_t_q>,
                meta::quote<Variant>>;

        template <template <typename...> typename Variant, typename... Fns>
        auto error_types_impl(completion_signatures<Fns...>) //
            -> meta::unpack_invoke< //
                meta::remove_q<void>::fn< //
                    decltype(detail::comp_sig_impl<set_error_t>(static_cast<Fns*>(nullptr)))...>,
                meta::quote<Variant>>;

        template <typename... Fns>
        constexpr bool sends_stopped_impl(completion_signatures<Fns...>) noexcept
        {
            return meta::contains_q<set_stopped_t()>::fn<Fns...>::value;
        }
    } // namespace detail

    namespace detail::comp_sig
    {
        struct no_completion_signatures
        {
        };

        template <typename T>
        using has_compl_sigs = std::enable_if_t<!std::is_same_v<T, no_completion_signatures>, T>;

        template <typename Sig, typename Env>
        concept dependent_sig = std::same_as<Sig, dependent_completion_signatures<no_env>> && std::same_as<Env, no_env>;
        template <typename Sig, typename Env>
        concept valid_completion_signatures = template_of<Sig, completion_signatures> || dependent_sig<Sig, Env>;

        struct get_completion_signatures_t
        {
            template <typename S, typename E>
            constexpr auto operator()(S&&, E&&) const noexcept
            {
                if constexpr (tag_invocable<get_completion_signatures_t, S, E>)
                {
                    using res_t = tag_invoke_result_t<get_completion_signatures_t, S, E>;
                    static_assert(template_of<res_t, completion_signatures> ||
                        template_of<res_t, dependent_completion_signatures>);
                    return res_t{};
                }
                else if constexpr (requires { typename std::remove_cvref_t<S>::completion_signatures; })
                {
                    using res_t = typename std::remove_cvref_t<S>::completion_signatures;
                    static_assert(template_of<res_t, completion_signatures> ||
                        template_of<res_t, dependent_completion_signatures>);
                    return res_t{};
                }
                else if constexpr (awaitable<S>)
                {
                    if constexpr (std::is_void_v<await_result_t<S>>)
                        return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
                    else
                        return completion_signatures< //
                            set_value_t(await_result_t<S>), set_error_t(std::exception_ptr), set_stopped_t()>{};
                }
                else
                    return no_completion_signatures{};
            }
        };
    } // namespace detail::comp_sig

    using detail::comp_sig::get_completion_signatures_t;
    inline constexpr get_completion_signatures_t get_completion_signatures{};

    template <typename S, typename E = no_env>
    using completion_signatures_of_t =
        detail::comp_sig::has_compl_sigs<call_result_t<get_completion_signatures_t, S, E>>;

    // clang-format off
    template <typename R>
    concept receiver =
        environment_provider<R> &&
        std::move_constructible<std::remove_cvref_t<R>> &&
        std::constructible_from<std::remove_cvref_t<R>, R>;
    // clang-format on

    namespace detail
    {
        template <typename R, typename Cpo, typename... Ts>
        constexpr bool recv_of_single_sig(Cpo (*)(Ts...)) noexcept
        {
            return nothrow_tag_invocable<Cpo, R, Ts...>;
        }

        template <typename R, typename... Sigs>
        constexpr bool recv_of_impl(completion_signatures<Sigs...>) noexcept
        {
            return (detail::recv_of_single_sig<R>(static_cast<Sigs*>(nullptr)) && ...);
        }
    } // namespace detail

    // clang-format off
    template <typename R, typename Sigs>
    concept receiver_of =
        receiver<R> &&
        detail::recv_of_impl<R>(Sigs{});
    // clang-format on

    namespace detail::recv_qry
    {
        struct forwarding_receiver_query_t
        {
            template <typename Cpo>
            constexpr bool operator()(const Cpo cpo) const noexcept
            {
                if constexpr (tag_invocable<forwarding_receiver_query_t, Cpo>)
                {
                    static_assert(nothrow_tag_invocable<forwarding_receiver_query_t, Cpo>,
                        "forwarding_receiver_query should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<forwarding_receiver_query_t, Cpo>>);
                    return tag_invoke(*this, cpo) ? true : false;
                }
                else
                    return !recvs::completion_cpo<Cpo>;
            }
        };

        // clang-format off
        template <typename T>
        concept fwd_recv_query = forwarding_receiver_query_t{}(T{});
        // clang-format on
    } // namespace detail::recv_qry

    using detail::recv_qry::forwarding_receiver_query_t;
    inline constexpr forwarding_receiver_query_t forwarding_receiver_query{};

    namespace detail::op_state
    {
        struct start_t
        {
            template <typename O>
                requires tag_invocable<start_t, O&>
            void operator()(O& ops) const noexcept
            {
                static_assert(nothrow_tag_invocable<start_t, O&>, "start should be noexcept");
                tag_invoke(*this, ops);
            }
        };
    } // namespace detail::op_state

    using detail::op_state::start_t;
    inline constexpr start_t start{};

    // clang-format off
    template <typename O>
    concept operation_state =
        std::destructible<O> &&
        std::is_object_v<O> &&
        requires(O& ops) { { exec::start(ops) } noexcept; };

    namespace detail
    {
        template <typename S, typename E>
        concept sender_base =
            requires { typename completion_signatures_of_t<S, E>; } &&
            comp_sig::valid_completion_signatures<completion_signatures_of_t<S, E>, E>;
    } // namespace detail

    template <typename S, typename E = no_env>
    concept sender =
        detail::sender_base<S, no_env> &&
        detail::sender_base<S, E> &&
        std::move_constructible<std::remove_cvref_t<S>>;
    // clang-format on

    namespace detail
    {
        template <typename... Ts>
        using decayed_tuple = std::tuple<std::decay_t<Ts>...>;

        struct empty_variant
        {
            empty_variant() = delete;
        };

        template <typename... Ts>
        using variant_or_empty = conditional_t<sizeof...(Ts) == 0, empty_variant,
            meta::unpack_invoke<meta::unique<std::decay_t<Ts>...>, meta::quote<std::variant>>>;
    } // namespace detail

    // clang-format off
    template <
        typename S, typename E = no_env,
        template <typename...> typename Tuple = detail::decayed_tuple,
        template <typename...> typename Variant = detail::variant_or_empty>
        requires sender<S, E>
    using value_types_of_t = decltype(detail::value_types_impl<Tuple, Variant>(completion_signatures_of_t<S, E>{}));

    template <
        typename S, typename E = no_env,
        template <typename...> typename Variant = detail::variant_or_empty>
        requires sender<S, E>
    using error_types_of_t = decltype(detail::error_types_impl<Variant>(completion_signatures_of_t<S, E>{}));

    template <typename S, typename E = no_env>
        requires sender<S, E>
    inline constexpr bool sends_stopped = detail::sends_stopped_impl(completion_signatures_of_t<S, E>{});
    
    template <typename S, typename E = no_env, typename... Ts>
    concept sender_of =
        sender<S, E> &&
        std::same_as<type_list<Ts...>, value_types_of_t<S, E, type_list, single_type_t>>;
    // clang-format on

    namespace detail
    {
        // clang-format off
        template <typename... Ts> struct collapse_types {};
        template <typename T> struct collapse_types<T> : std::type_identity<T> {};
        template <> struct collapse_types<> : std::type_identity<void> {};
        template <typename... Ts> using collapse_types_t = typename collapse_types<Ts...>::type;
        // clang-format on

        template <typename T>
        using comp_sig_of_single = conditional_t<std::is_void_v<T>, set_value_t(), set_value_t(with_regular_void_t<T>)>;
    } // namespace detail

    namespace detail::coro_utils
    {
        struct as_awaitable_t;
    }

    using detail::coro_utils::as_awaitable_t;
    extern const as_awaitable_t as_awaitable;

    namespace detail::conn
    {
        struct connect_t;

        // clang-format off
        template <typename R, typename S>
        concept receiver_for =
            sender<S, env_of_t<R>> &&
            receiver_of<R, completion_signatures_of_t<S, env_of_t<R>>>;

        template <typename S, typename R>
        concept tag_connectable =
            tag_invocable<connect_t, S, R> &&
            receiver_for<R, S>;
        // clang-format on

        template <typename S, typename R>
        class ops_task
        {
        public:
            class promise_type
            {
            public:
                template <typename S2, typename R2>
                promise_type(S2&& snd, R2&& recv): snd_(snd), recv_(recv)
                {
                }

                coro::suspend_always initial_suspend() const noexcept { return {}; }
                coro::suspend_never final_suspend() const noexcept { unreachable(); }
                void return_void() const noexcept {}
                ops_task get_return_object() { return ops_task(*this); }

                coro::coroutine_handle<> unhandled_stopped() const noexcept
                {
                    exec::set_stopped(static_cast<R&&>(recv_));
                    return coro::noop_coroutine();
                }

                template <typename F>
                auto yield_value(F&& func) noexcept
                {
                    struct res_t
                    {
                        std::decay_t<F> f;

                        bool await_ready() const noexcept { return false; }
                        void await_suspend(coro::coroutine_handle<>) { f(); }
                        void await_resume() const noexcept { unreachable(); }
                    };
                    return res_t{static_cast<F&&>(func)};
                }

                template <typename T>
                decltype(auto) await_transform(T&& value)
                {
                    if constexpr (tag_invocable<as_awaitable_t, T, promise_type&>)
                        return clu::tag_invoke(as_awaitable, static_cast<T&&>(value), *this);
                    else
                        return static_cast<T&&>(value);
                }

                void unhandled_exception() const noexcept { std::terminate(); }

            private:
                S& snd_;
                R& recv_;

                template <typename Cpo, typename... Args>
                    requires recv_qry::fwd_recv_query<Cpo> && callable<Cpo, const R&, Args...>
                friend decltype(auto) tag_invoke(const Cpo cpo, const promise_type& self, Args&&... args) noexcept(
                    nothrow_callable<Cpo, const R&, Args...>)
                {
                    return cpo(self.recv_, static_cast<Args&&>(args)...);
                }
            };

        private:
            unique_coroutine_handle<promise_type> handle_;

            explicit ops_task(promise_type& promise):
                handle_(coro::coroutine_handle<promise_type>::from_promise(promise))
            {
            }

            friend void tag_invoke(start_t, ops_task& self) noexcept { self.handle_.get().resume(); }
        };

        template <typename S, typename R>
        using conn_await_promise = typename ops_task<std::decay_t<S>, std::decay_t<R>>::promise_type;

        template <typename S, typename R>
        using await_comp_sigs = completion_signatures<comp_sig_of_single<await_result_t<S, conn_await_promise<S, R>>>,
            set_error_t(std::exception_ptr), set_stopped_t()>;

        // clang-format off
        template <typename S, typename R>
            requires receiver_of<R, await_comp_sigs<S, R>>
        ops_task<S, R> connect_awaitable(S snd, R recv)
        {
            std::exception_ptr eptr;
            try
            {
                if constexpr (std::is_void_v<await_result_t<S, conn_await_promise<S, R>>>)
                {
                    co_await std::move(snd);
                    co_yield [&] { exec::set_value(std::move(recv)); };
                }
                else
                {
                    auto&& res = co_await std::move(snd);
                    co_yield [&] { exec::set_value(std::move(recv), static_cast<decltype(res)>(res)); };
                }
            }
            catch (...)
            {
                eptr = std::current_exception();
            }
            co_yield [&] { exec::set_error(std::move(recv), std::move(eptr)); };
        }

        template <typename S, typename R>
        concept await_connectable = awaitable<S, conn_await_promise<S, R>>;
        
        template <typename S, typename R>
        concept connectable =
            receiver<R> &&
            (tag_connectable<S, R> || await_connectable<S, R>);

        template <typename S, typename R>
        concept nothrow_connectable =
            connectable<S, R> &&
            nothrow_tag_invocable<connect_t, S, R>;
        // clang-format on

        struct connect_t
        {
            template <typename S, typename R>
                requires connectable<S, R>
            auto operator()(S&& snd, R&& recv) const noexcept(nothrow_connectable<S, R>)
            {
                if constexpr (tag_connectable<S, R>)
                {
                    static_assert(operation_state<tag_invoke_result_t<connect_t, S, R>>,
                        "return type of connect should satisfy operation_state");
                    return tag_invoke(*this, static_cast<S&&>(snd), static_cast<R&&>(recv));
                }
                else
                    return conn::connect_awaitable(static_cast<S&&>(snd), static_cast<R&&>(recv));
            }
        };
    } // namespace detail::conn

    using detail::conn::connect_t;
    inline constexpr connect_t connect{};

    template <typename S, typename R>
    using connect_result_t = call_result_t<connect_t, S, R>;

    namespace detail
    {
        namespace snd_qry
        {
            template <recvs::completion_cpo Cpo>
            struct get_completion_scheduler_t;
        }

        namespace sched
        {
            struct schedule_t;
        }
    } // namespace detail

    using detail::snd_qry::get_completion_scheduler_t;
    using detail::sched::schedule_t;
    extern const schedule_t schedule;

    // clang-format off
    template <typename S, typename R>
    concept sender_to = detail::conn::receiver_for<R, S> && callable<connect_t, S, R>;

    template <typename S>
    concept scheduler =
        requires(S&& schd, const get_completion_scheduler_t<set_value_t>& cpo)
        {
            { exec::schedule(static_cast<S&&>(schd)) };
            {
                tag_invoke(cpo, exec::schedule(static_cast<S&&>(schd)))
            } -> std::same_as<std::remove_cvref_t<S>>;
        } &&
        std::copy_constructible<std::remove_cvref_t<S>> &&
        std::equality_comparable<std::remove_cvref_t<S>>;
    // clang-format on

    namespace detail
    {
        // clang-format off
        template <typename T>
        concept constexpr_boolean = requires { { T::value } -> boolean_testable; };
        template <typename T>
        concept constexpr_true = constexpr_boolean<T> && (T::value ? true : false);
        // clang-format on

        template <typename T, typename U>
        constexpr auto bool_and([[maybe_unused]] const T lhs, [[maybe_unused]] const U rhs) noexcept
        {
            if constexpr (constexpr_boolean<T> && constexpr_boolean<U>)
                return std::bool_constant<(T::value && U::value)>{};
            else
                return lhs && rhs;
        }
    } // namespace detail

    namespace detail::snd_qry
    {
        struct forwarding_sender_query_t
        {
            template <typename Cpo>
            constexpr bool operator()(const Cpo cpo) const noexcept
            {
                if constexpr (tag_invocable<forwarding_sender_query_t, Cpo>)
                {
                    static_assert(nothrow_tag_invocable<forwarding_sender_query_t, Cpo>,
                        "forwarding_sender_query should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<forwarding_sender_query_t, Cpo>>);
                    return tag_invoke(*this, cpo) ? true : false;
                }
                else
                    return false;
            }
        };

        // clang-format off
        template <typename T>
        concept fwd_snd_query = forwarding_sender_query_t{}(T{});
        // clang-format on

        template <recvs::completion_cpo Cpo>
        struct get_completion_scheduler_t
        {
            template <sender S>
                requires tag_invocable<get_completion_scheduler_t, S>
            auto operator()(const S& snd) const noexcept
            {
                static_assert(nothrow_tag_invocable<get_completion_scheduler_t, S>,
                    "get_completion_scheduler should be noexcept");
                static_assert(scheduler<tag_invoke_result_t<get_completion_scheduler_t, S>>,
                    "return type of get_completion_scheduler should satisfy scheduler");
                return tag_invoke(*this, snd);
            }

            // clang-format off
            constexpr friend bool tag_invoke(
                forwarding_sender_query_t, get_completion_scheduler_t) noexcept { return true; }
            // clang-format on
        };

        struct no_await_thunk_t
        {
            template <typename S>
            constexpr auto operator()([[maybe_unused]] const S& snd) const noexcept
            {
                if constexpr (tag_invocable<no_await_thunk_t, const S&>)
                {
                    static_assert(nothrow_tag_invocable<no_await_thunk_t, const S&>, //
                        "no_await_thunk should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<no_await_thunk_t, const S&>>);
                    return tag_invoke(*this, snd);
                }
                else
                    return std::false_type{};
            }
        };

        struct completes_inline_t
        {
            template <typename S>
            constexpr auto operator()([[maybe_unused]] const S& snd) const noexcept
            {
                if constexpr (tag_invocable<completes_inline_t, const S&>)
                {
                    static_assert(nothrow_tag_invocable<completes_inline_t, const S&>, //
                        "completes_inline should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<completes_inline_t, const S&>>);
                    return tag_invoke(*this, snd);
                }
                else
                    return std::false_type{};
            }

            // clang-format off
            constexpr friend bool tag_invoke(
                forwarding_sender_query_t, completes_inline_t) noexcept { return true; }
            // clang-format on
        };
    } // namespace detail::snd_qry

    using detail::snd_qry::forwarding_sender_query_t;
    using detail::snd_qry::get_completion_scheduler_t;
    using detail::snd_qry::no_await_thunk_t;
    using detail::snd_qry::completes_inline_t;
    inline constexpr forwarding_sender_query_t forwarding_sender_query{};
    template <typename Cpo>
    inline constexpr get_completion_scheduler_t<Cpo> get_completion_scheduler{};
    inline constexpr no_await_thunk_t no_await_thunk{};
    inline constexpr completes_inline_t completes_inline{};
    template <typename S>
    concept no_await_thunk_sender = sender<S> && detail::constexpr_true<call_result_t<no_await_thunk_t, const S&>>;
    template <typename S>
    concept inline_sender = sender<S> && detail::constexpr_true<call_result_t<completes_inline_t, const S&>>;

    namespace detail
    {
        template <typename Cpo, sender S>
        using completion_scheduler_of_t = call_result_t<get_completion_scheduler_t<Cpo>, S>;

        namespace wo_thunk
        {
            template <typename S>
            struct member_comp_sigs
            {
            };
            template <typename S>
                requires requires { typename S::completion_signatures; }
            struct member_comp_sigs<S>
            {
                using completion_signatures = typename S::completion_signatures;
            };

            template <typename S>
            struct snd_t_
            {
                class type;
            };

            template <typename S>
            using snd_t = typename snd_t_<std::remove_cvref_t<S>>::type;

            template <typename S>
            class snd_t_<S>::type : public member_comp_sigs<S>
            {
            public:
                // clang-format off
                template <forwarding<S> S2>
                explicit type(S2&& snd): snd_(static_cast<S2&&>(snd)) {}
                // clang-format on

            private:
                S snd_;

                template <typename Cpo, forwarding<type> Self, typename... Args>
                    requires tag_invocable<Cpo, copy_cvref_t<Self, S>, Args...>
                constexpr friend decltype(auto) tag_invoke(Cpo, Self&& self, Args&&... args) noexcept(
                    nothrow_tag_invocable<Cpo, copy_cvref_t<Self, S>, Args...>)
                {
                    return clu::tag_invoke(Cpo{}, static_cast<Self&&>(self).snd_, static_cast<Args&&>(args)...);
                }

                constexpr friend std::true_type tag_invoke(no_await_thunk_t, const type&) noexcept { return {}; }
            };

            // Wrap up a sender to use no await thunk
            struct without_await_thunk_t
            {
                template <sender S>
                auto operator()(S&& snd) const
                {
                    return snd_t<S>(static_cast<S&&>(snd));
                }
                constexpr auto operator()() const noexcept { return make_piper(*this); }
            };
        } // namespace wo_thunk

        namespace sched
        {
            struct schedule_t
            {
                template <typename S>
                    requires tag_invocable<schedule_t, S>
                auto operator()(S&& schd) const
                {
                    static_assert(
                        sender<tag_invoke_result_t<schedule_t, S>>, "return type of schedule should satisfy sender");
                    return wo_thunk::without_await_thunk_t{}(tag_invoke(*this, static_cast<S&&>(schd)));
                }
            };
        } // namespace sched

        namespace read
        {
            template <typename Cpo, typename R>
            struct ops_t_
            {
                struct type;
            };

            template <typename Cpo>
            struct snd_t_
            {
                struct type;
            };

            template <typename Cpo, typename R>
            struct ops_t_<Cpo, R>::type
            {
                R recv;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    if constexpr (nothrow_callable<Cpo, env_of_t<R>>)
                    {
                        auto value = Cpo{}(get_env(self.recv));
                        set_value(std::move(self.recv), std::move(value));
                    }
                    else
                    {
                        try
                        {
                            auto value = Cpo{}(get_env(self.recv));
                            set_value(std::move(self.recv), std::move(value));
                        }
                        catch (...)
                        {
                            set_error(std::move(self.recv, std::current_exception()));
                        }
                    }
                }
            };

            template <typename Cpo>
            struct snd_t_<Cpo>::type
            {
                template <receiver R>
                friend auto tag_invoke(connect_t, type, R&& recv)
                {
                    using ops_t = typename ops_t_<Cpo, std::remove_cvref_t<R>>::type;
                    return ops_t{static_cast<R&&>(recv)};
                }

                template <typename Env>
                friend auto tag_invoke(get_completion_signatures_t, type, Env&&)
                {
                    return dependent_completion_signatures<std::remove_cvref_t<Env>>{};
                }

                // clang-format off
                template <typename Env> requires
                    (!similar_to<Env, no_env>) &&
                    callable<Cpo, Env>
                friend auto tag_invoke(get_completion_signatures_t, type, Env&&)
                {
                    using res_t = call_result_t<Cpo, Env&&>;
                    if constexpr (nothrow_callable<Cpo, Env&&>)
                        return completion_signatures<set_value_t(res_t)>{};
                    else
                        return completion_signatures<set_value_t(res_t), set_error_t(std::exception_ptr)>{};
                }
                // clang-format on
            };

            struct read_t
            {
                template <typename Cpo>
                constexpr auto operator()(Cpo) const noexcept
                {
                    using snd_t = typename snd_t_<Cpo>::type;
                    return snd_t{};
                }
            };
        } // namespace read
    } // namespace detail

    using detail::wo_thunk::without_await_thunk_t;
    using detail::read::read_t;
    inline constexpr without_await_thunk_t without_await_thunk{};
    inline constexpr schedule_t schedule{};
    inline constexpr read_t read{};

    template <scheduler S>
    using schedule_result_t = call_result_t<schedule_t, S>;

    // clang-format off
    template <typename T>
    concept time_point =
        std::regular<T> &&
        std::totally_ordered<T> &&
        requires { typename T::duration; } &&
        requires(T tp, const T ctp, typename T::duration d)
        {
            { ctp + d } -> std::same_as<T>;
            { ctp - d } -> std::same_as<T>;
            { ctp - ctp } -> std::same_as<typename T::duration>;
            { tp += d } -> std::same_as<T&>;
            { tp -= d } -> std::same_as<T&>;
        };

    template <typename D>
    concept duration = template_of<D, std::chrono::duration>;
    // clang-format on

    namespace detail::time_schd
    {
        struct now_t
        {
            template <typename S>
                requires tag_invocable<now_t, S>
            auto operator()(S&& schd) const noexcept(nothrow_tag_invocable<now_t, S>)
            {
                static_assert(
                    time_point<tag_invoke_result_t<now_t, S>>, "return type of now should satisfy time_point");
                return tag_invoke(*this, static_cast<S&&>(schd));
            }
        };

        struct schedule_at_t
        {
            template <typename S, time_point T>
                requires tag_invocable<schedule_at_t, S, T>
            auto operator()(S&& schd, const T tp) const
            {
                static_assert(sender<tag_invoke_result_t<schedule_at_t, S, T>>, //
                    "return type of schedule_at should satisfy sender");
                return tag_invoke(*this, static_cast<S&&>(schd), tp);
            }
        };

        struct schedule_after_t
        {
            template <typename S, typename D>
                requires tag_invocable<schedule_after_t, S, D>
            auto operator()(S&& schd, const D dur) const
            {
                static_assert(sender<tag_invoke_result_t<schedule_after_t, S, D>>, //
                    "return type of schedule_after should satisfy sender");
                return tag_invoke(*this, static_cast<S&&>(schd), dur);
            }
        };
    } // namespace detail::time_schd

    using detail::time_schd::now_t;
    using detail::time_schd::schedule_at_t;
    using detail::time_schd::schedule_after_t;
    inline constexpr now_t now{};
    inline constexpr schedule_at_t schedule_at{};
    inline constexpr schedule_after_t schedule_after{};

    // clang-format off
    template <typename S>
    concept time_scheduler =
        scheduler<S> &&
        requires(S&& schd) { exec::now(static_cast<S&&>(schd)); } &&
        requires(S&& schd, decltype(exec::now(static_cast<S&&>(schd))) tp)
        {
            exec::schedule_at(static_cast<S&&>(schd), tp);
            exec::schedule_after(static_cast<S&&>(schd), tp - tp);
        };
    // clang-format on

    namespace detail
    {
        template <typename E>
        std::exception_ptr make_exception_ptr(E&& error) noexcept
        {
            if constexpr (decays_to<E, std::exception_ptr>)
                return static_cast<E&&>(error);
            else if constexpr (decays_to<E, std::error_code>)
                return std::make_exception_ptr(std::system_error(error));
            else
                return std::make_exception_ptr(static_cast<E&&>(error));
        }
    } // namespace detail

    namespace detail::coro_utils
    {
        template <typename S, typename E = no_env>
        using single_sender_value_type = value_types_of_t<S, E, collapse_types_t, collapse_types_t>;

        // clang-format off
        template <typename S, typename E = no_env>
        concept single_sender =
            sender<S, E> &&
            requires { typename single_sender_value_type<S, E>; };
        // clang-format on

        template <typename S, typename P>
        using result_t = with_regular_void_t<single_sender_value_type<S, env_of_t<P>>>;

        template <typename S, typename P>
        using variant_t = std::variant<monostate, result_t<S, P>, std::exception_ptr>;

        template <typename S, typename P>
        struct recv_t_
        {
            class type;
        };

        template <typename S, typename P>
        using awaitable_receiver = typename recv_t_<S, std::remove_cvref_t<P>>::type;

        template <typename S, typename P>
        class recv_t_<S, P>::type
        {
        public:
            type(variant_t<S, P>* ptr, const coro::coroutine_handle<P> handle): result_(ptr), handle_(handle) {}

        private:
            variant_t<S, P>* result_;
            coro::coroutine_handle<P> handle_;

            template <typename... Ts>
                requires std::constructible_from<result_t<S, P>, Ts...>
            friend void tag_invoke(set_value_t, type&& self, Ts&&... args) noexcept
            {
                try
                {
                    self.result_->template emplace<1>(static_cast<Ts&&>(args)...);
                }
                catch (...)
                {
                    self.result_->template emplace<2>(std::current_exception());
                }
                self.handle_.resume();
            }

            template <typename E>
            friend void tag_invoke(set_error_t, type&& self, E&& error) noexcept
            {
                self.result_->template emplace<2>(detail::make_exception_ptr(static_cast<E&&>(error)));
                self.handle_.resume();
            }

            friend void tag_invoke(set_stopped_t, type&& self) noexcept
            {
                coro::coroutine_handle<> cont = self.handle_.promise().unhandled_stopped();
                cont.resume();
            }

            // @formatter:off
            template <typename Cpo, typename... Args>
                requires recv_qry::fwd_recv_query<Cpo> && callable<Cpo, const P&, Args...>
            friend decltype(auto) tag_invoke(const Cpo cpo, const type& self, Args&&... args) noexcept(
                nothrow_callable<Cpo, const P&, Args...>)
            {
                return cpo(std::as_const(self.handle_.promise()), static_cast<Args&&>(args)...);
            }
            // @formatter:on
        };

        template <typename S, typename P>
        struct awt_t_
        {
            class type;
        };

        template <typename S, typename P>
        using sender_awaitable = typename awt_t_<std::remove_cvref_t<S>, std::remove_cvref_t<P>>::type;

        template <typename S, typename P>
        class awt_t_<S, P>::type
        {
        public:
            template <typename S2>
            type(S2&& snd, P& promise):
                state_(exec::connect(static_cast<S2&&>(snd),
                    awaitable_receiver<S, P>(&result_, coro::coroutine_handle<P>::from_promise(promise))))
            {
            }

            // TODO: if the sender completes inline, return true from await_ready()
            bool await_ready() const noexcept { return inline_sender<S>; }
            void await_suspend(coro::coroutine_handle<P>) noexcept { exec::start(state_); }
            auto await_resume()
            {
                if (result_.index() == 2)
                    std::rethrow_exception(std::get<2>(result_));
                if constexpr (!std::is_void_v<value_t>)
                    return static_cast<value_t&&>(std::get<1>(result_));
            }

        private:
            using value_t = single_sender_value_type<S, env_of_t<P>>;

            variant_t<S, P> result_{};
            connect_result_t<S, awaitable_receiver<S, P>> state_;
        };

        // clang-format off
        template <typename S, typename P>
        concept awaitable_sender =
            single_sender<S, env_of_t<P>> &&
            sender_to<S, awaitable_receiver<S, P>> &&
            requires(P& p) { { p.unhandled_stopped() } -> std::convertible_to<coro::coroutine_handle<>>; };
        // clang-format on

        struct as_awaitable_t
        {
            template <typename T, typename P>
                requires std::is_lvalue_reference_v<P>
            decltype(auto) operator()(T&& value, P&& prms) const
            {
                if constexpr (tag_invocable<as_awaitable_t, T, P>)
                {
                    static_assert(awaitable<tag_invoke_result_t<as_awaitable_t, T, P>>,
                        "customizations to as_awaitable should return an awaitable");
                    return clu::tag_invoke(*this, static_cast<T&&>(value), prms);
                }
                else if constexpr (awaitable<T> || !awaitable_sender<T, P>)
                    return static_cast<T&&>(value);
                else
                    return sender_awaitable<T, P>(static_cast<T&&>(value), prms);
            }
        };
    } // namespace detail::coro_utils

    inline constexpr as_awaitable_t as_awaitable{};

    namespace detail
    {
        template <typename... Args>
        using default_set_value = completion_signatures<set_value_t(Args...)>;

        template <typename Err>
        using default_set_error = completion_signatures<set_error_t(Err)>;

        template <typename... Sigs>
        using join_sigs =
            meta::unpack_invoke<meta::unique_l<meta::flatten<Sigs...>>, meta::quote<completion_signatures>>;

        template <typename SetError>
        struct join_err_sigs
        {
            template <typename... Errs>
            using fn = join_sigs<meta::invoke<SetError, Errs>...>;
        };

        template <typename Sndr, class Env, typename AddlSigs, //
            typename SetValue, typename SetError, typename SetStopped>
        auto make_comp_sigs_impl(priority_tag<1>)
            -> join_sigs<AddlSigs, value_types_of_t<Sndr, Env, SetValue::template fn, join_sigs>,
                error_types_of_t<Sndr, Env, join_err_sigs<SetError>::template fn>,
                conditional_t<sends_stopped<Sndr, Env>, SetStopped, completion_signatures<>>>;

        template <typename...>
        auto make_comp_sigs_impl(priority_tag<0>) -> dependent_completion_signatures<no_env>;
    } // namespace detail

    template <sender Sndr, class Env = no_env,
        detail::comp_sig::valid_completion_signatures<Env> AddlSigs = completion_signatures<>,
        template <typename...> typename SetValue = detail::default_set_value,
        template <typename> typename SetError = detail::default_set_error,
        detail::comp_sig::valid_completion_signatures<Env> SetStopped = completion_signatures<set_stopped_t()>>
        requires sender<Sndr, Env>
    using make_completion_signatures = decltype(detail::make_comp_sigs_impl< //
        Sndr, Env, AddlSigs, meta::quote<SetValue>, meta::quote1<SetError>, SetStopped>(priority_tag<1>{}));

    namespace detail::gnrl_qry
    {
        struct get_scheduler_t
        {
            // clang-format off
            template <typename R> requires
                (!std::same_as<R, no_env>) &&
                tag_invocable<get_scheduler_t, R>
            auto operator()(const R& r) const noexcept
            // clang-format on
            {
                static_assert(nothrow_tag_invocable<get_scheduler_t, R>, "get_scheduler should be noexcept");
                static_assert(scheduler<tag_invoke_result_t<get_scheduler_t, R>>,
                    "return type of get_scheduler should satisfy scheduler");
                return tag_invoke(*this, r);
            }

            constexpr auto operator()() const noexcept { return exec::read(*this); }
        };

        struct get_delegatee_scheduler_t
        {
            // clang-format off
            template <typename R> requires
                (!std::same_as<R, no_env>) &&
                tag_invocable<get_delegatee_scheduler_t, R>
            auto operator()(const R& r) const noexcept
            // clang-format on
            {
                static_assert(
                    nothrow_tag_invocable<get_delegatee_scheduler_t, R>, "get_delegatee_scheduler should be noexcept");
                static_assert(scheduler<tag_invoke_result_t<get_delegatee_scheduler_t, R>>,
                    "return type of get_delegatee_scheduler should satisfy scheduler");
                return tag_invoke(*this, r);
            }

            constexpr auto operator()() const noexcept { return exec::read(*this); }
        };

        struct get_allocator_t
        {
            template <typename R>
                requires(!std::same_as<R, no_env>)
            auto operator()(const R& r) const noexcept
            {
                if constexpr (tag_invocable<get_allocator_t, R>)
                {
                    static_assert(nothrow_tag_invocable<get_allocator_t, R>, "get_allocator should be noexcept");
                    static_assert(allocator<tag_invoke_result_t<get_allocator_t, R>>,
                        "return type of get_allocator should satisfy Allocator");
                    return tag_invoke(*this, r);
                }
                else
                    return std::allocator<std::byte>{};
            }

            constexpr auto operator()() const noexcept { return exec::read(*this); }
        };

        struct get_stop_token_t
        {
            template <typename R>
                requires(!std::same_as<R, no_env>)
            auto operator()(const R& r) const noexcept
            {
                if constexpr (tag_invocable<get_stop_token_t, R>)
                {
                    static_assert(nothrow_tag_invocable<get_stop_token_t, R>, "get_stop_token should be noexcept");
                    static_assert(stoppable_token<tag_invoke_result_t<get_stop_token_t, R>>,
                        "return type of get_stop_token should satisfy stoppable_token");
                    return tag_invoke(*this, r);
                }
                else
                    return never_stop_token{};
            }

            constexpr auto operator()() const noexcept { return exec::read(*this); }
        };
    } // namespace detail::gnrl_qry

    using detail::gnrl_qry::get_scheduler_t;
    using detail::gnrl_qry::get_delegatee_scheduler_t;
    using detail::gnrl_qry::get_allocator_t;
    using detail::gnrl_qry::get_stop_token_t;
    inline constexpr get_scheduler_t get_scheduler{};
    inline constexpr get_delegatee_scheduler_t get_delegatee_scheduler{};
    inline constexpr get_allocator_t get_allocator{};
    inline constexpr get_stop_token_t get_stop_token{};

    template <typename T>
    using stop_token_of_t = std::remove_cvref_t<call_result_t<get_stop_token_t, T>>;

    enum class forwarding_progress_guarantee
    {
        concurrent,
        parallel,
        weakly_parallel
    };

    namespace detail::schd_qry
    {
        struct forwarding_scheduler_query_t
        {
            template <typename Cpo>
            constexpr bool operator()(const Cpo cpo) const noexcept
            {
                if constexpr (tag_invocable<forwarding_scheduler_query_t, Cpo>)
                {
                    static_assert(nothrow_tag_invocable<forwarding_scheduler_query_t, Cpo>,
                        "forwarding_scheduler_query should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<forwarding_scheduler_query_t, Cpo>>);
                    return tag_invoke(*this, cpo) ? true : false;
                }
                else
                    return true;
            }
        };

        struct get_forward_progress_guarantee_t
        {
            template <scheduler S>
            constexpr forwarding_progress_guarantee operator()(const S& schd) const noexcept
            {
                if constexpr (tag_invocable<get_forward_progress_guarantee_t, const S&>)
                {
                    static_assert(nothrow_tag_invocable<get_forward_progress_guarantee_t, const S&>,
                        "get_forward_progress_guarantee should be noexcept");
                    static_assert(std::same_as<tag_invoke_result_t<get_forward_progress_guarantee_t, const S&>,
                        forwarding_progress_guarantee>);
                    return tag_invoke(*this, schd);
                }
                else
                    return forwarding_progress_guarantee::weakly_parallel;
            }
        };
    } // namespace detail::schd_qry

    using detail::schd_qry::forwarding_scheduler_query_t;
    using detail::schd_qry::get_forward_progress_guarantee_t;
    inline constexpr forwarding_scheduler_query_t forwarding_scheduler_query{};
    inline constexpr get_forward_progress_guarantee_t get_forward_progress_guarantee{};
} // namespace clu::exec

namespace clu::this_thread
{
    namespace detail::this_thr_qry
    {
        struct execute_may_block_caller_t
        {
            template <exec::scheduler S>
            constexpr bool operator()(const S& schd) const noexcept
            {
                if constexpr (tag_invocable<execute_may_block_caller_t, const S&>)
                {
                    static_assert(nothrow_tag_invocable<execute_may_block_caller_t, const S&>,
                        "execute_may_block_caller should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<execute_may_block_caller_t, const S&>>);
                    return tag_invoke(*this, schd) ? true : false;
                }
                else
                    return true;
            }
        };
    } // namespace detail::this_thr_qry

    using detail::this_thr_qry::execute_may_block_caller_t;
    inline constexpr execute_may_block_caller_t execute_may_block_caller{};
} // namespace clu::this_thread
