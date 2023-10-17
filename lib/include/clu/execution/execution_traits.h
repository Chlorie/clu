#pragma once

#include <variant>

#include "awaitable_traits.h"
#include "../assertion.h"
#include "../stop_token.h"
#include "../tag_invoke.h"
#include "../meta_list.h"
#include "../meta_algorithm.h"
#include "../unique_coroutine_handle.h"
#include "../function_traits.h"
#include "../piper.h"
#include "../query.h"

namespace clu::exec
{
    namespace detail
    {
        struct set_value_t
        {
            template <typename R, typename... Vals>
                requires tag_invocable<set_value_t, R, Vals...>
            CLU_STATIC_CALL_OPERATOR(void)(R&& recv, Vals&&... vals) noexcept
            {
                static_assert(nothrow_tag_invocable<set_value_t, R, Vals...>, "set_value should be noexcept");
                tag_invoke(set_value_t{}, static_cast<R&&>(recv), static_cast<Vals&&>(vals)...);
            }
        };

        struct set_error_t
        {
            template <typename R, typename Err>
                requires tag_invocable<set_error_t, R, Err>
            CLU_STATIC_CALL_OPERATOR(void)(R&& recv, Err&& err) noexcept
            {
                static_assert(nothrow_tag_invocable<set_error_t, R, Err>, "set_error should be noexcept");
                tag_invoke(set_error_t{}, static_cast<R&&>(recv), static_cast<Err&&>(err));
            }
        };

        struct set_stopped_t
        {
            template <typename R>
                requires tag_invocable<set_stopped_t, R>
            CLU_STATIC_CALL_OPERATOR(void)(R&& recv) noexcept
            {
                static_assert(nothrow_tag_invocable<set_stopped_t, R>, "set_stopped should be noexcept");
                tag_invoke(set_stopped_t{}, static_cast<R&&>(recv));
            }
        };

        template <typename Cpo>
        concept completion_cpo = same_as_any_of<Cpo, set_value_t, set_error_t, set_stopped_t>;
    } // namespace detail

    using detail::set_value_t;
    using detail::set_error_t;
    using detail::set_stopped_t;
    inline constexpr set_value_t set_value{};
    inline constexpr set_error_t set_error{};
    inline constexpr set_stopped_t set_stopped{};

    namespace detail
    {
        template <std::same_as<set_value_t> Cpo, typename... Args>
        type_list<Args...> comp_sig_impl(Cpo (*)(Args...));
        template <std::same_as<set_error_t> Cpo, typename Err>
        Err comp_sig_impl(Cpo (*)(Err));
        template <std::same_as<set_stopped_t> Cpo>
        bool comp_sig_impl(Cpo (*)());
        template <typename T = void>
        void comp_sig_impl(...);

        template <typename Cpo, template <typename...> typename Tuple, typename... Args>
        type_tag_t<meta::invoke_unquoted<Tuple, Args...>> gather_sigs_impl(Cpo (*)(Args...));
        template <typename Cpo, template <typename...> typename Tuple>
        void gather_sigs_impl(...);

        template <typename Fn>
        concept completion_signature =
            (!std::same_as<decltype(detail::comp_sig_impl(static_cast<Fn*>(nullptr))), void>);
    } // namespace detail

    template <detail::completion_signature... Fns>
    struct completion_signatures
    {
    };

    namespace detail
    {
        struct as_awaitable_t;
    }

    using detail::as_awaitable_t;
    extern const as_awaitable_t as_awaitable;

    namespace detail
    {
        template <typename E>
        struct env_promise
        {
            E env;

            const E& tag_invoke(get_env_t) const noexcept { return env; }

            template <typename A>
            decltype(auto) await_transform(A&& a)
            {
                if constexpr (tag_invocable<as_awaitable_t, A, env_promise&>)
                    return clu::tag_invoke(as_awaitable, static_cast<A&&>(a), *this);
                else
                    return static_cast<A&&>(a);
            }
        };
    } // namespace detail

    namespace detail
    {
        template <typename Sigs>
        concept valid_completion_signatures = template_of<Sigs, completion_signatures>;

        struct get_completion_signatures_t
        {
        private:
            template <typename S, typename E>
            constexpr static auto impl() noexcept
            {
                if constexpr (tag_invocable<get_completion_signatures_t, S, E>)
                {
                    using res_t = tag_invoke_result_t<get_completion_signatures_t, S, E>;
                    if constexpr (valid_completion_signatures<res_t>)
                        return res_t{};
                }
                else if constexpr (requires { typename std::remove_cvref_t<S>::completion_signatures; })
                {
                    using res_t = typename std::remove_cvref_t<S>::completion_signatures;
                    if constexpr (valid_completion_signatures<res_t>)
                        return res_t{};
                }
                else if constexpr (awaitable<S, env_promise<E>>)
                {
                    using promise = env_promise<E>;
                    if constexpr (std::is_void_v<await_result_t<S, promise>>)
                        return completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>{};
                    else
                        return completion_signatures<set_value_t(await_result_t<S, promise>), //
                            set_error_t(std::exception_ptr), set_stopped_t()>{};
                }
            }

        public:
            template <typename S, typename E>
                requires(!std::is_void_v<decltype(impl<S, E>())>)
            constexpr CLU_STATIC_CALL_OPERATOR(auto)(S&&, E&&) noexcept
            {
                return impl<S, E>();
            }
        };
    } // namespace detail

    using detail::get_completion_signatures_t;
    inline constexpr get_completion_signatures_t get_completion_signatures{};

    template <typename R>
    inline constexpr bool enable_receiver = requires { typename R::is_receiver; };

    template <typename R>
    concept receiver = //
        enable_receiver<std::remove_cvref_t<R>> && //
        environment_provider<R> && //
        std::move_constructible<std::remove_cvref_t<R>> && //
        std::constructible_from<std::remove_cvref_t<R>, R>;

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

    template <typename R, typename Sigs>
    concept receiver_of = receiver<R> && detail::recv_of_impl<R>(Sigs{});

    namespace detail
    {
        struct start_t
        {
            template <typename O>
                requires tag_invocable<start_t, O&>
            CLU_STATIC_CALL_OPERATOR(void)(O& ops) noexcept
            {
                static_assert(nothrow_tag_invocable<start_t, O&>, "start should be noexcept");
                tag_invoke(start_t{}, ops);
            }
        };
    } // namespace detail

    using detail::start_t;
    inline constexpr start_t start{};

    template <typename O>
    concept operation_state = //
        queryable<O> && //
        std::is_object_v<O> && //
        requires(O& ops) {
            {
                exec::start(ops)
            } noexcept;
        };

    template <typename S>
    inline constexpr bool enable_sender = requires { typename S::is_sender; };
    template <awaitable<detail::env_promise<empty_env>> S>
    inline constexpr bool enable_sender<S> = true;

    template <typename S>
    concept sender = //
        enable_sender<std::remove_cvref_t<S>> && //
        environment_provider<S> && //
        std::move_constructible<std::remove_cvref_t<S>> && //
        std::constructible_from<std::remove_cvref_t<S>, S>;

    template <typename S, typename E = empty_env>
    concept sender_in = sender<S> && callable<get_completion_signatures_t, S, E>;

    template <typename S, typename E = empty_env>
        requires sender_in<S, E>
    using completion_signatures_of_t = call_result_t<get_completion_signatures_t, S, E>;

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

        template <typename Cpo, template <typename...> typename Tuple>
        struct gather_single_sig
        {
            template <typename Sig>
            using fn = decltype(detail::gather_sigs_impl<Cpo, Tuple>(static_cast<Sig*>(nullptr)));
        };

        template <typename Cpo, typename S, typename E, //
            template <typename...> typename Tuple, template <typename...> typename Variant>
            requires sender_in<S, E>
        using gather_signatures = //
            meta::unpack_invoke< //
                meta::transform_l< //
                    meta::remove_l< //
                        meta::transform_l< //
                            completion_signatures_of_t<S, E>, //
                            meta::requote<gather_single_sig<Cpo, Tuple>>>,
                        void>,
                    meta::_t_q>,
                meta::quote<Variant>>;

        template <typename Sig>
        struct sender_of_impl
        {
            using cpo = typename function_traits<Sig>::return_type;
            template <typename... Ts>
            using fn = cpo(Ts...);
        };
    } // namespace detail

    template <typename S, typename E = empty_env, //
        template <typename...> typename Tuple = detail::decayed_tuple,
        template <typename...> typename Variant = detail::variant_or_empty>
        requires sender_in<S, E>
    using value_types_of_t = detail::gather_signatures<set_value_t, S, E, Tuple, Variant>;

    template <typename S, typename E = empty_env, //
        template <typename...> typename Variant = detail::variant_or_empty>
        requires sender_in<S, E>
    using error_types_of_t = detail::gather_signatures<set_error_t, S, E, std::type_identity_t, Variant>;

    template <typename S, typename E = empty_env>
        requires sender_in<S, E>
    inline constexpr bool sends_stopped = meta::contains_lv<completion_signatures_of_t<S, E>, set_stopped_t()>;

    template <typename S, typename Sig, typename E = empty_env>
    concept sender_of = //
        sender_in<S, E> && //
        detail::completion_signature<Sig> &&
        std::same_as<type_list<Sig>,
            detail::gather_signatures<typename detail::sender_of_impl<Sig>::cpo, S, E,
                detail::sender_of_impl<Sig>::template fn, type_list>>;

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

        struct connect_t;

        template <typename S, typename R>
        concept tag_connectable = //
            sender_in<S, env_of_t<R>> && //
            receiver_of<R, completion_signatures_of_t<S, env_of_t<R>>> && //
            tag_invocable<connect_t, S, R>;

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

                decltype(auto) tag_invoke(get_env_t) const //
                    noexcept(nothrow_callable<get_env_t, const R&>)
                {
                    return get_env(recv_);
                }

            private:
                S& snd_;
                R& recv_;
            };

            void tag_invoke(start_t) noexcept { handle_.get().resume(); }

        private:
            unique_coroutine_handle<promise_type> handle_;

            explicit ops_task(promise_type& promise):
                handle_(coro::coroutine_handle<promise_type>::from_promise(promise))
            {
            }
        };

        template <typename S, typename R>
        using conn_await_promise = typename ops_task<std::decay_t<S>, std::decay_t<R>>::promise_type;

        template <typename S, typename R>
        using await_comp_sigs = completion_signatures< //
            comp_sig_of_single<await_result_t<S, conn_await_promise<S, R>>>, //
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
            CLU_STATIC_CALL_OPERATOR(auto)(S&& snd, R&& recv) noexcept(nothrow_connectable<S, R>)
            {
                if constexpr (tag_connectable<S, R>)
                {
                    static_assert(operation_state<tag_invoke_result_t<connect_t, S, R>>,
                        "connect should return an operation_state");
                    return tag_invoke(connect_t{}, static_cast<S&&>(snd), static_cast<R&&>(recv));
                }
                else
                    return detail::connect_awaitable(static_cast<S&&>(snd), static_cast<R&&>(recv));
            }
        };
    } // namespace detail

    using detail::connect_t;
    inline constexpr connect_t connect{};

    template <typename S, typename R>
    using connect_result_t = call_result_t<connect_t, S, R>;

    template <typename S, typename R>
    concept sender_to = //
        sender_in<S, env_of_t<R>> && //
        receiver_of<R, completion_signatures_of_t<S, env_of_t<R>>> && //
        callable<connect_t, S, R>;

    namespace detail
    {
        template <completion_cpo Cpo>
        struct get_completion_scheduler_t;
        struct schedule_t;
    } // namespace detail

    using detail::get_completion_scheduler_t;
    using detail::schedule_t;
    extern const schedule_t schedule;

    // clang-format off
    template <typename S>
    concept scheduler =
        queryable<S> &&
        requires(S&& schd, const get_completion_scheduler_t<set_value_t>& cpo)
        {
            { schedule(static_cast<S&&>(schd)) };
            { tag_invoke(cpo, get_env(schedule(static_cast<S&&>(schd)))) } -> std::same_as<std::remove_cvref_t<S>>;
        } &&
        std::copy_constructible<std::remove_cvref_t<S>> && // TODO: enforce noexcept
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

        template <completion_cpo Cpo>
        struct get_completion_scheduler_t
        {
            template <typename E>
                requires tag_invocable<get_completion_scheduler_t, E>
            CLU_STATIC_CALL_OPERATOR(auto)(const E& env) noexcept
            {
                static_assert(nothrow_tag_invocable<get_completion_scheduler_t, E>,
                    "get_completion_scheduler should be noexcept");
                static_assert(scheduler<tag_invoke_result_t<get_completion_scheduler_t, E>>,
                    "get_completion_scheduler should return a scheduler");
                return clu::tag_invoke(get_completion_scheduler_t{}, env);
            }

            CLU_FORWARDING_QUERY(get_completion_scheduler_t);
        };

        struct no_await_thunk_t
        {
            template <typename E>
            constexpr CLU_STATIC_CALL_OPERATOR(auto)([[maybe_unused]] const E& env) noexcept
            {
                if constexpr (tag_invocable<no_await_thunk_t, const E&>)
                {
                    static_assert(nothrow_tag_invocable<no_await_thunk_t, const E&>, //
                        "no_await_thunk should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<no_await_thunk_t, const E&>>);
                    return tag_invoke(no_await_thunk_t{}, env);
                }
                else
                    return std::false_type{};
            }
        };

        struct completes_inline_t
        {
            template <typename E>
            constexpr CLU_STATIC_CALL_OPERATOR(auto)([[maybe_unused]] const E& env) noexcept
            {
                if constexpr (tag_invocable<completes_inline_t, const E&>)
                {
                    static_assert(nothrow_tag_invocable<completes_inline_t, const E&>, //
                        "completes_inline should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<completes_inline_t, const E&>>);
                    return tag_invoke(completes_inline_t{}, env);
                }
                else
                    return std::false_type{};
            }

            // TODO: forward this query?
        };
    } // namespace detail

    using detail::get_completion_scheduler_t;
    using detail::no_await_thunk_t;
    using detail::completes_inline_t;
    template <typename Cpo>
    inline constexpr get_completion_scheduler_t<Cpo> get_completion_scheduler{};
    inline constexpr no_await_thunk_t no_await_thunk{};
    inline constexpr completes_inline_t completes_inline{};

    template <typename S>
    concept no_await_thunk_sender = //
        sender<S> && detail::constexpr_true<call_result_t<no_await_thunk_t, env_of_t<S>>>;

    template <typename S>
    concept inline_sender = //
        sender<S> && detail::constexpr_true<call_result_t<completes_inline_t, env_of_t<S>>>;

    namespace detail
    {
        template <typename Cpo, sender S>
        using completion_scheduler_of_t = call_result_t<get_completion_scheduler_t<Cpo>, S>;

        namespace wo_thunk
        {
            template <typename S>
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <forwarding<S> S2>
                explicit snd_t_(S2&& snd): snd_(static_cast<S2&&>(snd)) {}
                // clang-format on

                auto tag_invoke(get_env_t) const
                    CLU_SINGLE_RETURN(clu::adapt_env(get_env(snd_), query_value{no_await_thunk, std::true_type{}}));

                // Forward all other CPO's
                template <typename Env>
                static auto tag_invoke(get_completion_signatures_t, Env&&)
                {
                    return completion_signatures_of_t<S, Env>{};
                }

                template <typename Cpo, typename... Args>
                    requires tag_invocable<Cpo, const S&, Args...> && (!std::same_as<Cpo, get_env_t>)
                constexpr decltype(auto)
                    tag_invoke(Cpo, Args&&... args) const& noexcept(nothrow_tag_invocable<Cpo, const S&, Args...>)
                {
                    return clu::tag_invoke(Cpo{}, snd_, static_cast<Args&&>(args)...);
                }

                template <typename Cpo, typename... Args>
                    requires tag_invocable<Cpo, S, Args...> && (!std::same_as<Cpo, get_env_t>)
                constexpr decltype(auto)
                    tag_invoke(Cpo, Args&&... args) && noexcept(nothrow_tag_invocable<Cpo, S, Args...>)
                {
                    return clu::tag_invoke(Cpo{}, std::move(snd_), static_cast<Args&&>(args)...);
                }

            private:
                S snd_;
            };

            template <typename S>
            using snd_t = snd_t_<std::remove_cvref_t<S>>;

            // Wrap up a sender to use no await thunk
            struct without_await_thunk_t
            {
                template <sender S>
                CLU_STATIC_CALL_OPERATOR(auto)
                (S&& snd)
                {
                    // Avoid wrapping the same sender multiple times
                    using result_t = conditional_t<no_await_thunk_sender<S>, std::remove_cvref_t<S>, snd_t<S>>;
                    return result_t(static_cast<S&&>(snd));
                }
                constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(without_await_thunk_t{}); }
            };
        } // namespace wo_thunk

        struct schedule_t
        {
            template <typename S>
                requires tag_invocable<schedule_t, S>
            CLU_STATIC_CALL_OPERATOR(auto)(S&& schd)
            {
                static_assert(
                    sender<tag_invoke_result_t<schedule_t, S>>, "return type of schedule should satisfy sender");
                return wo_thunk::without_await_thunk_t{}(tag_invoke(schedule_t{}, static_cast<S&&>(schd)));
            }
        };

        template <typename Qry, typename R>
        struct read_ops
        {
            R recv;

            void tag_invoke(start_t) & noexcept
            {
                if constexpr (nothrow_callable<Qry, env_of_t<R>>)
                {
                    auto value = Qry{}(get_env(recv));
                    set_value(std::move(recv), std::move(value));
                }
                else
                {
                    try
                    {
                        auto value = Qry{}(get_env(recv));
                        set_value(std::move(recv), std::move(value));
                    }
                    catch (...)
                    {
                        set_error(std::move(recv, std::current_exception()));
                    }
                }
            }
        };

        template <typename Qry>
        struct read_snd
        {
            using is_sender = void;

            template <receiver R>
            auto tag_invoke(connect_t, R&& recv) const
            {
                return read_ops<Qry, std::remove_cvref_t<R>>{static_cast<R&&>(recv)};
            }

            template <typename Env>
                requires callable<Qry, Env>
            auto tag_invoke(get_completion_signatures_t, Env&&) const
            {
                using res_t = call_result_t<Qry, Env&&>;
                if constexpr (nothrow_callable<Qry, Env&&>)
                    return completion_signatures<set_value_t(res_t)>{};
                else
                    return completion_signatures<set_value_t(res_t), set_error_t(std::exception_ptr)>{};
            }
        };

        struct read_t
        {
            template <typename Qry>
            constexpr CLU_STATIC_CALL_OPERATOR(auto)(Qry) noexcept
            {
                return read_snd<Qry>{};
            }
        };

        template <typename R>
        class just_void_ops_t
        {
        public:
            // clang-format off
            template <forwarding<R> R2>
            explicit just_void_ops_t(R2&& recv) noexcept(std::is_nothrow_constructible_v<R, R2>):
                recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void tag_invoke(start_t) noexcept { set_value(static_cast<R&&>(recv_)); }

        private:
            R recv_;
        };

        struct just_void_snd_t
        {
            using is_sender = void;

            // clang-format off
            constexpr static completion_signatures<set_value_t()> tag_invoke(
                get_completion_signatures_t, auto&&) noexcept { return {}; }
            // clang-format on

            template <typename R>
            auto tag_invoke(connect_t, R&& recv) const
                CLU_SINGLE_RETURN(just_void_ops_t<std::decay_t<R>>(static_cast<R&&>(recv)));
        };

        inline constexpr just_void_snd_t just_void{};
    } // namespace detail

    using detail::wo_thunk::without_await_thunk_t;
    using detail::read_t;
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
    // clang-format on

    template <typename D>
    concept duration = template_of<D, std::chrono::duration>;

    namespace detail
    {
        struct now_t
        {
            template <typename S>
                requires tag_invocable<now_t, S>
            CLU_STATIC_CALL_OPERATOR(auto)(S&& schd) noexcept(nothrow_tag_invocable<now_t, S>)
            {
                static_assert(
                    time_point<tag_invoke_result_t<now_t, S>>, "return type of now should satisfy time_point");
                return tag_invoke(now_t{}, static_cast<S&&>(schd));
            }
        };

        struct schedule_at_t
        {
            template <typename S, time_point T>
                requires tag_invocable<schedule_at_t, S, T>
            CLU_STATIC_CALL_OPERATOR(auto)(S&& schd, const T tp)
            {
                static_assert(sender<tag_invoke_result_t<schedule_at_t, S, T>>, //
                    "return type of schedule_at should satisfy sender");
                return without_await_thunk(tag_invoke(schedule_at_t{}, static_cast<S&&>(schd), tp));
            }

            CLU_STATIC_CALL_OPERATOR(auto)(const time_point auto tp)
            {
                return clu::make_piper( //
                    clu::bind_back(schedule_at_t{}, tp));
            }
        };

        struct schedule_after_t
        {
            template <typename S, duration D>
                requires tag_invocable<schedule_after_t, S, D>
            CLU_STATIC_CALL_OPERATOR(auto)(S&& schd, const D dur)
            {
                static_assert(sender<tag_invoke_result_t<schedule_after_t, S, D>>, //
                    "return type of schedule_after should satisfy sender");
                return without_await_thunk(tag_invoke(schedule_after_t{}, static_cast<S&&>(schd), dur));
            }

            CLU_STATIC_CALL_OPERATOR(auto)(const duration auto dur)
            {
                return clu::make_piper( //
                    clu::bind_back(schedule_after_t{}, dur));
            }
        };
    } // namespace detail

    using detail::now_t;
    using detail::schedule_at_t;
    using detail::schedule_after_t;
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

        template <typename S, typename E>
        using single_sender_value_type = std::decay_t<value_types_of_t<S, E, collapse_types_t, collapse_types_t>>;

        // clang-format off
        template <typename S, typename E>
        concept single_sender = sender_in<S, E> && requires { typename single_sender_value_type<S, E>; };
        // clang-format on

        template <typename S, typename P>
        using single_sender_result_t = with_regular_void_t<single_sender_value_type<S, env_of_t<P>>>;

        template <typename S, typename P>
        using single_sender_variant_t = std::variant<monostate, single_sender_result_t<S, P>, std::exception_ptr>;

        template <typename S, typename P>
        class awaitable_recv
        {
        public:
            using is_receiver = void;

            awaitable_recv(single_sender_variant_t<S, P>* ptr, const coro::coroutine_handle<P> handle):
                result_(ptr), handle_(handle)
            {
            }

            template <typename... Ts>
                requires std::constructible_from<single_sender_result_t<S, P>, Ts...>
            void tag_invoke(set_value_t, Ts&&... args) && noexcept
            {
                // clang-format off
                try { result_->template emplace<1>(static_cast<Ts&&>(args)...); }
                catch (...) { result_->template emplace<2>(std::current_exception()); }
                // clang-format on
                handle_.resume();
            }

            template <typename E>
            void tag_invoke(set_error_t, E&& error) && noexcept
            {
                result_->template emplace<2>(detail::make_exception_ptr(static_cast<E&&>(error)));
                handle_.resume();
            }

            void tag_invoke(set_stopped_t) && noexcept
            {
                coro::coroutine_handle<> cont = handle_.promise().unhandled_stopped();
                cont.resume();
            }

            auto tag_invoke(get_env_t) const noexcept { return get_env(handle_.promise()); }

        private:
            single_sender_variant_t<S, P>* result_;
            coro::coroutine_handle<P> handle_;
        };

        template <typename S, typename P>
        class sender_awaitable
        {
        public:
            template <typename S2>
            sender_awaitable(S2&& snd, P& promise):
                state_(exec::connect(static_cast<S2&&>(snd),
                    awaitable_recv<S, std::remove_cvref_t<P>>(
                        &result_, coro::coroutine_handle<P>::from_promise(promise))))
            {
            }

            // TODO: if the sender completes inline, return true from await_ready()
            bool await_ready() const noexcept { return false; }
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

            single_sender_variant_t<S, P> result_{};
            connect_result_t<S, awaitable_recv<S, P>> state_;
        };

        // clang-format off
        template <typename P>
        concept stop_handler = requires(P& p)
        {
            { p.unhandled_stopped() } -> std::convertible_to<coro::coroutine_handle<>>;
        };
        // clang-format on

        template <typename S, typename P>
        concept awaitable_sender = //
            single_sender<S, env_of_t<P>> && //
            sender_to<S, awaitable_recv<S, std::remove_cvref_t<P>>> && //
            stop_handler<P>;

        struct as_awaitable_t
        {
            template <typename T, typename P>
                requires std::is_lvalue_reference_v<P>
            CLU_STATIC_CALL_OPERATOR(decltype(auto))(T&& value, P&& prms)
            {
                if constexpr (tag_invocable<as_awaitable_t, T, P>)
                {
                    static_assert(awaitable<tag_invoke_result_t<as_awaitable_t, T, P>>,
                        "customizations to as_awaitable should return an awaitable");
                    return clu::tag_invoke(as_awaitable_t{}, static_cast<T&&>(value), prms);
                }
                else if constexpr (awaitable<T> || !awaitable_sender<T, P>)
                    return static_cast<T&&>(value);
                else
                    return sender_awaitable<std::remove_cvref_t<T>, std::remove_cvref_t<P>>(
                        static_cast<T&&>(value), prms);
            }
        };
    } // namespace detail

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
        auto make_comp_sigs_impl(priority_tag<0>) -> void;
    } // namespace detail

    template <sender Sndr, class Env = empty_env,
        detail::valid_completion_signatures AddlSigs = completion_signatures<>,
        template <typename...> typename SetValue = detail::default_set_value,
        template <typename> typename SetError = detail::default_set_error,
        detail::valid_completion_signatures SetStopped = completion_signatures<set_stopped_t()>>
        requires sender_in<Sndr, Env>
    using make_completion_signatures = decltype(detail::make_comp_sigs_impl< //
        Sndr, Env, AddlSigs, meta::quote<SetValue>, meta::quote<SetError>, SetStopped>(priority_tag<1>{}));
} // namespace clu::exec

namespace clu
{
    namespace detail
    {
        struct get_allocator_t
        {
            CLU_FORWARDING_QUERY(get_allocator_t);

            template <typename R>
            CLU_STATIC_CALL_OPERATOR(auto)
            (const R& r) noexcept
            {
                if constexpr (tag_invocable<get_allocator_t, R>)
                {
                    static_assert(nothrow_tag_invocable<get_allocator_t, R>, "get_allocator should be noexcept");
                    static_assert(allocator<tag_invoke_result_t<get_allocator_t, R>>,
                        "return type of get_allocator should satisfy Allocator");
                    return clu::tag_invoke(get_allocator_t{}, r);
                }
                else
                    return std::allocator<std::byte>{}; // Note: not to spec (should be ill-formed)
            }

            constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return exec::read(get_allocator_t{}); }
        };

        struct get_stop_token_t
        {
            CLU_FORWARDING_QUERY(get_stop_token_t);

            template <typename R>
            CLU_STATIC_CALL_OPERATOR(auto)
            (const R& r) noexcept
            {
                if constexpr (tag_invocable<get_stop_token_t, R>)
                {
                    static_assert(nothrow_tag_invocable<get_stop_token_t, R>, "get_stop_token should be noexcept");
                    static_assert(stoppable_token<tag_invoke_result_t<get_stop_token_t, R>>,
                        "return type of get_stop_token should satisfy stoppable_token");
                    return clu::tag_invoke(get_stop_token_t{}, r);
                }
                else
                    return never_stop_token{};
            }

            constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return exec::read(get_stop_token_t{}); }
        };
    } // namespace detail

    using detail::get_allocator_t;
    using detail::get_stop_token_t;
    inline constexpr get_allocator_t get_allocator{};
    inline constexpr get_stop_token_t get_stop_token{};

    template <typename T>
    using allocator_of_t = std::remove_cvref_t<call_result_t<get_allocator_t, T>>;
    template <typename T>
    using stop_token_of_t = std::remove_cvref_t<call_result_t<get_stop_token_t, T>>;
} // namespace clu

namespace clu::exec
{
    namespace detail
    {
        struct get_scheduler_t
        {
            CLU_FORWARDING_QUERY(get_scheduler_t);

            template <typename R>
                requires tag_invocable<get_scheduler_t, R>
            CLU_STATIC_CALL_OPERATOR(auto)(const R& r) noexcept
            {
                static_assert(nothrow_tag_invocable<get_scheduler_t, R>, "get_scheduler should be noexcept");
                static_assert(scheduler<tag_invoke_result_t<get_scheduler_t, R>>,
                    "return type of get_scheduler should satisfy scheduler");
                return clu::tag_invoke(get_scheduler_t{}, r);
            }

            constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return exec::read(get_scheduler_t{}); }
        };

        struct get_delegatee_scheduler_t
        {
            CLU_FORWARDING_QUERY(get_delegatee_scheduler_t);

            template <typename R>
                requires tag_invocable<get_delegatee_scheduler_t, R>
            CLU_STATIC_CALL_OPERATOR(auto)(const R& r) noexcept
            {
                static_assert(
                    nothrow_tag_invocable<get_delegatee_scheduler_t, R>, "get_delegatee_scheduler should be noexcept");
                static_assert(scheduler<tag_invoke_result_t<get_delegatee_scheduler_t, R>>,
                    "return type of get_delegatee_scheduler should satisfy scheduler");
                return clu::tag_invoke(get_delegatee_scheduler_t{}, r);
            }

            constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return exec::read(get_delegatee_scheduler_t{}); }
        };

    } // namespace detail

    using detail::get_scheduler_t;
    using detail::get_delegatee_scheduler_t;
    inline constexpr get_scheduler_t get_scheduler{};
    inline constexpr get_delegatee_scheduler_t get_delegatee_scheduler{};

    enum class forwarding_progress_guarantee
    {
        concurrent,
        parallel,
        weakly_parallel
    };

    namespace detail
    {
        struct get_forward_progress_guarantee_t
        {
            template <scheduler S>
            constexpr CLU_STATIC_CALL_OPERATOR(forwarding_progress_guarantee)(const S& schd) noexcept
            {
                if constexpr (tag_invocable<get_forward_progress_guarantee_t, const S&>)
                {
                    static_assert(nothrow_tag_invocable<get_forward_progress_guarantee_t, const S&>,
                        "get_forward_progress_guarantee should be noexcept");
                    static_assert(std::same_as<tag_invoke_result_t<get_forward_progress_guarantee_t, const S&>,
                        forwarding_progress_guarantee>);
                    return tag_invoke(get_forward_progress_guarantee_t{}, schd);
                }
                else
                    return forwarding_progress_guarantee::weakly_parallel;
            }
        };
    } // namespace detail

    using detail::get_forward_progress_guarantee_t;
    inline constexpr get_forward_progress_guarantee_t get_forward_progress_guarantee{};

    namespace detail
    {
        struct next_t
        {
            template <typename S>
                requires tag_invocable<next_t, S&>
            CLU_STATIC_CALL_OPERATOR(auto)(S& stream) //
                noexcept(nothrow_tag_invocable<next_t, S&>)
            {
                static_assert(sender<tag_invoke_result_t<next_t, S&>>, //
                    "next(stream) must produce a sender");
                return tag_invoke(next_t{}, stream);
            }
        };

        struct cleanup_t
        {
            template <typename S>
            CLU_STATIC_CALL_OPERATOR(auto)
            (S& stream) noexcept
            {
                if constexpr (tag_invocable<cleanup_t, S&>)
                {
                    static_assert(nothrow_tag_invocable<cleanup_t, S&>, //
                        "cleanup(stream) must not throw");
                    using sender_t = tag_invoke_result_t<cleanup_t, S&>;
                    static_assert(sender_of<sender_t, set_value_t()>, //
                        "cleanup(stream) must produce a sender that sends nothing "
                        "(like clu::task<void>)");
                    return tag_invoke(cleanup_t{}, stream);
                }
                else
                    return just_void;
            }
        };
    } // namespace detail

    using detail::next_t;
    using detail::cleanup_t;
    inline constexpr next_t next{};
    inline constexpr cleanup_t cleanup{};

    template <typename S>
    concept stream = callable<next_t, S&> && callable<cleanup_t, S&>;
    template <stream S>
    using next_result_t = call_result_t<next_t, S&>;
    template <stream S>
    using cleanup_result_t = call_result_t<cleanup_t, S&>;
} // namespace clu::exec

namespace clu::this_thread
{
    namespace detail
    {
        struct execute_may_block_caller_t
        {
            template <exec::scheduler S>
            constexpr CLU_STATIC_CALL_OPERATOR(bool)(const S& schd) noexcept
            {
                if constexpr (tag_invocable<execute_may_block_caller_t, const S&>)
                {
                    static_assert(nothrow_tag_invocable<execute_may_block_caller_t, const S&>,
                        "execute_may_block_caller should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<execute_may_block_caller_t, const S&>>);
                    return tag_invoke(execute_may_block_caller_t{}, schd) ? true : false;
                }
                else
                    return true;
            }
        };
    } // namespace detail

    using detail::execute_may_block_caller_t;
    inline constexpr execute_may_block_caller_t execute_may_block_caller{};
} // namespace clu::this_thread
