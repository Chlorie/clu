#pragma once

#include <variant>

#include "awaitable_traits.h"
#include "../stop_token.h"
#include "../tag_invoke.h"
#include "../meta_list.h"
#include "../meta_algorithm.h"
#include "../unique_coroutine_handle.h"

namespace clu::exec
{
    // --- Execution Environments ---

    struct no_env
    {
        friend void tag_invoke(auto, no_env, auto&&...) = delete;
    };

    inline struct get_env_t
    {
        template <typename R>
        constexpr decltype(auto) operator()(R&& recv) const
        {
            if constexpr (tag_invocable<get_env_t, R>)
                return clu::tag_invoke(*this, static_cast<R&&>(recv));
            else
                return empty_env{};
        }

    private:
        struct empty_env {};
    } constexpr get_env{};

    template <typename R>
    using env_of_t = decltype(get_env(std::declval<R>()));

    inline struct forwarding_env_query_t
    {
        template <typename Cpo>
        constexpr bool operator()(Cpo&& cpo) const noexcept
        {
            if constexpr (tag_invocable<forwarding_env_query_t, Cpo>)
            {
                static_assert(
                    std::same_as<tag_invoke_result_t<forwarding_env_query_t, Cpo>, bool> &&
                    nothrow_tag_invocable<forwarding_env_query_t, Cpo>,
                    "forwarding_env_query should be noexcept and return bool");
                return clu::tag_invoke(*this, static_cast<Cpo&&>(cpo));
            }
            return true;
        }
    } constexpr forwarding_env_query{};

    // --- Receivers ---

    // @formatter:off
    inline struct set_value_t
    {
        template <typename R, typename... Vs> requires
            tag_invocable<set_value_t, R&&, Vs&&...> &&
            std::same_as<tag_invoke_result_t<set_value_t, R&&, Vs&&...>, void>
        constexpr void operator()(R&& recv, Vs&&... values) const
            noexcept(nothrow_tag_invocable<set_value_t, R&&, Vs&&...>)
        {
            clu::tag_invoke(*this,
                static_cast<R&&>(recv), static_cast<Vs&&>(values)...);
        }
    } constexpr set_value{};

    inline struct set_error_t
    {
        template <typename R, typename E> requires
            tag_invocable<set_error_t, R&&, E&&> &&
            std::same_as<tag_invoke_result_t<set_error_t, R&&, E&>, void>
        constexpr void operator()(R&& recv, E&& error) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<set_error_t, R&&, E&&>, 
                "set_error should be noexcept");
            clu::tag_invoke(*this,
                static_cast<R&&>(recv), static_cast<E&&>(error));
        }
    } constexpr set_error{};

    inline struct set_stopped_t
    {
        template <typename R> requires
            tag_invocable<set_stopped_t, R&&> &&
            std::same_as<tag_invoke_result_t<set_stopped_t, R&&>, void>
        constexpr void operator()(R&& recv) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<set_stopped_t, R&&>, 
                "set_done should be noexcept");
            clu::tag_invoke(*this, static_cast<R&&>(recv));
        }
    } constexpr set_stopped{};

    namespace detail
    {
        template <typename T>
        concept completion_cpo = same_as_any_of<T, set_value_t, set_error_t, set_stopped_t>;
    }

    template <typename R, typename E = std::exception_ptr>
    concept receiver =
        std::move_constructible<std::remove_cvref_t<R>> &&
        std::constructible_from<std::remove_cvref_t<R>, R> &&
        requires(std::remove_cvref_t<R>&& recv, E&& error)
        {
            { exec::set_stopped(std::move(recv)) } noexcept;
            { exec::set_error(std::move(recv), static_cast<E&&>(error)) } noexcept;
        };

    template <typename R, typename... Vs>
    concept receiver_of =
        receiver<R> &&
        requires(std::remove_cvref_t<R>&& recv, Vs&&... values)
        {
            exec::set_value(std::move(recv), static_cast<Vs&&>(values)...);
        };

    namespace detail
    {
        template <typename R, typename T>
        concept receiver_of_single_type =
            receiver_of<R, T> ||
            (std::same_as<T, void> && receiver_of<R>);
    }
    // @formatter:on

    template <typename... Vs, receiver_of<Vs...> R>
    void try_set_value(R&& recv, Vs&&... values) noexcept
    {
        try { exec::set_value(static_cast<R&&>(recv), static_cast<Vs&&>(values)...); }
        catch (...) { exec::set_error(static_cast<R&&>(recv), std::current_exception()); }
    }

    inline struct forwarding_receiver_query_t
    {
        template <typename Cpo>
        constexpr bool operator()(Cpo&& cpo) const noexcept
        {
            if constexpr (tag_invocable<forwarding_receiver_query_t, Cpo>)
            {
                static_assert(
                    std::same_as<tag_invoke_result_t<forwarding_receiver_query_t, Cpo>, bool> &&
                    nothrow_tag_invocable<forwarding_receiver_query_t, Cpo>,
                    "forwarding_receiver_query_t should be noexcept and return bool");
                return clu::tag_invoke(*this, static_cast<Cpo&&>(cpo));
            }
            else if constexpr (detail::completion_cpo<std::remove_cvref_t<Cpo>>)
                return false;
            else
                return true;
        }
    } constexpr forwarding_receiver_query{};

    // --- Operation States ---

    inline struct start_t
    {
        template <typename O>
            requires tag_invocable<start_t, O&>
        constexpr void operator()(O& op) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<start_t, O&>,
                "start should be noexcept");
            (void)clu::tag_invoke(*this, static_cast<O&>(op));
        }
    } constexpr start{};

    // @formatter:off
    template <typename O>
    concept operation_state =
        std::destructible<O> &&
        std::is_object_v<O> &&
        requires(O& op) { { exec::start(op) } noexcept; };
    // @formatter:on

    // --- Senders ---

    namespace detail
    {
        struct empty_variant
        {
            empty_variant() = delete;
        };

        template <typename... Ts>
        using variant_or_empty = conditional_t<
            sizeof...(Ts) == 0,
            empty_variant,
            std::variant<Ts...>
        >;

        template <typename... Ts>
        using decayed_tuple = std::tuple<std::decay_t<Ts>...>;
    }

    struct no_completion_signatures {};

    namespace detail
    {
        template <typename F> struct compl_sig_impl : std::false_type {};

        template <typename... Ts>
        struct compl_sig_impl<set_value_t(Ts ...)> : std::true_type
        {
            using cpo = set_value_t;
            using type = type_list<Ts...>;
        };

        template <typename E>
        struct compl_sig_impl<set_error_t(E)> : std::true_type
        {
            using cpo = set_error_t;
            using type = E;
        };

        template <> struct compl_sig_impl<set_stopped_t()> : std::true_type
        {
            using cpo = set_stopped_t;
        };

        template <typename Cpo>
        struct matching_cpo
        {
            template <typename Sig>
            using fn = std::bool_constant<std::is_same_v<typename Sig::cpo, Cpo>>;
        };

        template <typename F> concept completion_signature = compl_sig_impl<F>::value;
    }

    template <detail::completion_signature... Sigs>
    struct completion_signatures
    {
        template <template <typename...> typename Tup, template <typename...> typename Var>
        using value_types = meta::unpack_invoke<
            meta::transform_l<
                meta::filter_q<detail::matching_cpo<set_value_t>>::fn<detail::compl_sig_impl<Sigs>...>,
                meta::compose_q<
                    meta::_t_q,
                    meta::bind_back_q1<meta::unpack_invoke_q, meta::quote<Tup>>
                >
            >,
            meta::quote<Var>
        >;

        template <template <typename...> typename Var>
        using error_types = meta::unpack_invoke<
            meta::transform_l<
                meta::filter_q<detail::matching_cpo<set_error_t>>::fn<detail::compl_sig_impl<Sigs>...>,
                meta::_t_q
            >,
            meta::quote<Var>
        >;

        static constexpr bool sends_stopped = meta::contains_q<set_stopped_t()>::fn<Sigs...>::value;
    };

    inline struct get_completion_signatures_t
    {
        template <typename S, typename E = no_env>
        constexpr auto operator()(S&&, E&&) const noexcept
        {
            if constexpr (tag_invocable<get_completion_signatures_t, S, E>)
                return tag_invoke_result_t<get_completion_signatures_t, S, E>{};
            else if constexpr (requires { typename std::remove_cvref_t<S>::completion_signatures; })
                return typename std::remove_cvref_t<S>::completion_signatures{};
            else if constexpr (awaitable<S>)
            {
                if constexpr (std::is_void_v<await_result_t<S>>)
                    return completion_signatures<
                        set_value_t(),
                        set_error_t(std::exception_ptr),
                        set_stopped_t()>{};
                else
                    return completion_signatures<
                        set_value_t(await_result_t<S>),
                        set_error_t(std::exception_ptr),
                        set_stopped_t>{};
            }
            else
                return no_completion_signatures{};
        }
    } constexpr get_completion_signatures{};

    template <typename S, typename E = no_env>
    using completion_signatures_of_t = decltype(
        get_completion_signatures(std::declval<S>(), std::declval<E>()));

    // @formatter:off
    namespace detail
    {
        template <template <template <typename...> typename, template <typename...> typename> typename>
        struct has_value_types {};
        template <template <template <typename...> typename> typename>
        struct has_error_types {};

        template <typename S>
        concept has_sender_types = requires
        {
            typename has_value_types<S::template value_types>;
            typename has_error_types<S::template error_types>;
            typename std::bool_constant<S::sends_stopped>;
        };

        template <typename S, typename E>
        concept sender_base =
            requires { typename completion_signatures_of_t<S, E>; } &&
            has_sender_types<completion_signatures_of_t<S, E>>;
    }

    template <typename S, typename E = no_env>
    concept sender =
        detail::sender_base<S, E> &&
        detail::sender_base<S, no_env> &&
        std::move_constructible<std::remove_cvref_t<S>>;

    template <
        typename S,
        typename E = no_env,
        template <typename...> typename Tuple = detail::decayed_tuple,
        template <typename...> typename Variant = detail::variant_or_empty>
        requires sender<S, E>
    using value_types_of_t =
        typename completion_signatures_of_t<S, E>::template value_types<Tuple, Variant>;

    template <
        typename S,
        typename E = no_env,
        template <typename...> typename Variant = detail::variant_or_empty>
        requires sender<S, E>
    using error_types_of_t =
        typename completion_signatures_of_t<S, E>::template error_types<Variant>;
    
    template <typename S, typename E = no_env, typename... Ts>
    concept sender_of =
        sender<S, E> &&
        std::same_as<
            type_list<Ts...>,
            value_types_of_t<S, E, type_list, single_type_t>>;
    // @formatter:on

    template <typename E> struct dependent_completion_signatures {};
    template <>
    struct dependent_completion_signatures<no_env>
    {
        template <template <typename...> typename, template <typename...> typename> requires false
        using value_types = void;
        template <template <typename...> typename> requires false
        using error_types = void;
        static constexpr bool sends_stopped = false;
    };

    namespace detail
    {
        template <typename... Vs>
        using default_set_value = set_value_t(Vs ...);
        template <typename E>
        using default_set_error = set_error_t(E);

        template <bool Stop>
        using stopped_list = conditional_t<Stop, type_list<set_stopped_t()>, type_list<>>;

        template <typename Val, typename Err>
        using transform_set_value_error_compl_sig_q = meta::compose_q<
            meta::quote1<compl_sig_impl>,
            meta::if_q<
                matching_cpo<set_value_t>,
                meta::compose_q<
                    meta::_t_q,
                    meta::bind_back_q1<meta::unpack_invoke_q, Val>
                >,
                meta::if_q<
                    matching_cpo<set_error_t>,
                    meta::compose_q<meta::_t_q, Err>
                >
            >
        >;

        // @formatter:off
        template <typename Snd, typename Env, typename More, typename Val, typename Err, bool Stop>
        using make_compl_sig_impl = meta::unpack_invoke<
            meta::unique_l<
                meta::remove_l<
                    meta::flatten<
                        meta::transform_l<
                            meta::remove_l<completion_signatures_of_t<Snd, Env>, set_stopped_t()>,
                            transform_set_value_error_compl_sig_q<Val, Err>
                        >,
                        stopped_list<Stop>,
                        More
                    >,
                    void
                >
            >,
            meta::quote<completion_signatures>
        >;
        // @formatter:on

        template <typename Snd, typename Env, typename More, typename Val, typename Err, bool Stop> requires true
        make_compl_sig_impl<Snd, Env, More, Val, Err, Stop> make_compl_sig_impl_func();
        template <typename, typename Env, typename, typename, typename, bool>
        dependent_completion_signatures<Env> make_compl_sig_impl_func();
    }

    template <
        sender Snd,
        typename Env = no_env,
        template_of<completion_signatures> AddlSigs = completion_signatures<>,
        template <typename...> typename SetValue = detail::default_set_value,
        template <typename> typename SetError = detail::default_set_error,
        bool SendsStopped = completion_signatures_of_t<Snd, Env>::sends_stopped>
        requires sender<Snd, Env>
    using make_completion_signatures = decltype(
        detail::make_compl_sig_impl_func<
            Snd, Env, AddlSigs,
            meta::quote<SetValue>, meta::quote1<SetError>,
            SendsStopped>());

    struct as_awaitable_t;
    extern const as_awaitable_t as_awaitable;
    struct connect_t;

    namespace detail
    {
        template <typename R>
        struct operation_state_task_
        {
            struct promise;

            class type
            {
            public:
                using promise_type = promise;
                explicit type(const coro::coroutine_handle<> handle): handle_(handle) {}
                friend void tag_invoke(start_t, type& self) noexcept { self.handle_.get().resume(); }
            private:
                unique_coroutine_handle<> handle_{};
            };
        };

        template <typename R>
        struct operation_state_task_<R>::promise
        {
            R& recv;

            template <typename S> promise(S&&, R& r): recv(r) {}

            coro::suspend_always initial_suspend() const noexcept { return {}; }
            [[noreturn]] coro::suspend_always final_suspend() const noexcept { std::terminate(); }
            [[noreturn]] void unhandled_exception() const noexcept { std::terminate(); }
            [[noreturn]] void return_void() const noexcept { std::terminate(); }

            auto unhandled_stopped() const
            {
                exec::set_stopped(static_cast<R&&>(recv));
                return coro::noop_coroutine();
            }

            template <typename A>
            A&& await_transform(A&& a) const noexcept { return static_cast<A&&>(a); }

            // @formatter:off
            template <typename A>
                requires callable<as_awaitable_t, A&&, promise&>
            auto await_transform(A&& a)
                noexcept(nothrow_callable<as_awaitable_t, A&&, promise&>)
                -> call_result_t<as_awaitable_t, A&&, promise&>
            {
                return as_awaitable(static_cast<A&&>(a), *this);
            }

            template <typename Cpo, typename... Args>
                requires callable<Cpo, const R&, Args&&...>
            auto tag_invoke(const Cpo cpo, Args&&... args)
                noexcept(nothrow_callable<Cpo, const R&, Args&&...>)
                -> call_result_t<Cpo, const R&, Args&&...>
            {
                return cpo(recv, static_cast<Args&&>(args)...);
            }
            // @formatter:on

            template <typename F>
            auto yield_value(F&& func) const
            {
                struct suspend_call
                {
                    F&& func;
                    bool await_ready() const noexcept { return false; }
                    void await_suspend(coro::coroutine_handle<>) const noexcept { static_cast<F&&>(func)(); }
                    [[noreturn]] void await_resume() const noexcept { std::terminate(); }
                };
                return suspend_call{ static_cast<F&&>(func) };
            }

            auto get_return_object() noexcept
            {
                const auto handle = coro::coroutine_handle<promise>::from_promise(*this);
                return type(handle);
            }
        };

        template <typename R>
        using operation_state_task = typename operation_state_task_<R>::type;

        template <typename S, typename R>
        using connect_await_result = await_result_t<
            std::decay_t<S>,
            typename operation_state_task<std::decay_t<R>>::promise_type
        >;

        template <typename S, typename R>
        static operation_state_task<R> connect_awaitable(S snd, R recv)
        {
            using res_t = connect_await_result<S, R>;
            std::exception_ptr eptr;
            try
            {
                if constexpr (std::is_void_v<res_t>)
                {
                    co_await static_cast<S&&>(snd);
                    co_yield [&] { exec::set_value(static_cast<R&&>(recv)); };
                }
                else
                {
                    auto&& result = co_await static_cast<S&&>(snd);
                    co_yield [&]
                    {
                        exec::set_value(
                            static_cast<R&&>(recv),
                            static_cast<decltype(result)>(result)
                        );
                    };
                }
            }
            catch (...) { eptr = std::current_exception(); }
            co_yield [&] { exec::set_error(static_cast<R&&>(recv), std::move(eptr)); };
        }

        // @formatter:off
        template <typename S, typename R>
        concept tag_connectable =
            sender<S> &&
            receiver<R> &&
            tag_invocable<connect_t, S, R>;

        template <typename S, typename R>
        concept await_connectable =
            (!tag_connectable<S, R>) &&
            awaitable<S, typename operation_state_task<std::decay_t<R>>::promise_type> &&
            (
                (std::is_void_v<connect_await_result<S, R>> && receiver_of<R>) ||
                receiver_of<R, connect_await_result<S, R>>
            );

        template <typename S, typename R>
        concept connectable =
            tag_connectable<S, R> ||
            await_connectable<S, R>;
        // @formatter:on
    }

    inline struct connect_t
    {
        template <typename S, receiver R>
            // requires detail::connectable<S, R>
            requires detail::tag_connectable<S, R>
        constexpr auto operator()(S&& snd, R&& recv) const
        {
            if constexpr (detail::tag_connectable<S, R>)
            {
                static_assert(
                    operation_state<tag_invoke_result_t<connect_t, S, R>>,
                    "return type of connect should satisfy operation_state");
                return tag_invoke(*this, static_cast<S&&>(snd), static_cast<R&&>(recv));
            }
            else
                return detail::connect_awaitable(static_cast<S&&>(snd), static_cast<R&&>(recv));
        }
    } constexpr connect{};

    template <typename S, typename R>
    using connect_result_t = decltype(connect(std::declval<S>(), std::declval<R>()));

    // @formatter:off
    template <typename S, typename R>
    concept sender_to =
        sender<S, env_of_t<R>> &&
        receiver<R> &&
        requires(S&& snd, R&& recv) { exec::connect(static_cast<S&&>(snd), static_cast<R&&>(recv)); };
    // @formatter:on

    inline struct forwarding_sender_query_t
    {
        template <typename Cpo>
        constexpr bool operator()(Cpo&& cpo) const noexcept
        {
            if constexpr (tag_invocable<forwarding_sender_query_t, Cpo>)
            {
                static_assert(
                    std::same_as<tag_invoke_result_t<forwarding_sender_query_t, Cpo>, bool> &&
                    nothrow_tag_invocable<forwarding_sender_query_t, Cpo>,
                    "forwarding_receiver_query_t should be noexcept and return bool");
                return clu::tag_invoke(*this, static_cast<Cpo&&>(cpo));
            }
            return false;
        }
    } constexpr forwarding_sender_query{};

    namespace detail
    {
        template <sender S>
        constexpr auto ssvt_impl() noexcept
        {
            using types = value_types_of_t<S, no_env, type_list, type_list>;
            if constexpr (types::size == 0)
                return type_tag<void>;
            else if constexpr (types::size == 1)
            {
                using single_type_q = meta::quote<single_type_t>;
                using tuple = meta::unpack_invoke<types, single_type_q>;
                if constexpr (tuple::size == 0)
                    return type_tag<void>;
                else
                    return type_tag<meta::unpack_invoke<tuple, single_type_q>>;
            }
        }

        template <sender S>
        using single_sender_value_type = typename decltype(detail::ssvt_impl<S>())::type;

        // @formatter:off
        template <typename S>
        concept single_typed_sender =
            sender<S> &&
            requires { typename single_sender_value_type<S>; };
        // @formatter:on

        template <typename S, typename P>
        struct awaitable_receiver_
        {
            class type;
        };

        template <typename S, typename P>
        class awaitable_receiver_<S, P>::type
        {
        public:
            using value_t = single_sender_value_type<S>;
            using result_t = with_regular_void_t<value_t>;
            using variant_t = std::variant<std::monostate, result_t, std::exception_ptr>;

            type(variant_t* ptr, const coro::coroutine_handle<P> cont): ptr_(ptr), cont_(cont) {}

            template <typename = int>
                requires std::is_void_v<value_t>
            friend void tag_invoke(set_value_t, type&& recv) noexcept
            {
                recv.ptr_->template emplace<1>();
                recv.cont_.resume();
            }

            template <forwarding<value_t> T = value_t>
                requires (!std::is_void_v<value_t>)
            friend void tag_invoke(set_value_t, type&& recv, T&& value)
            {
                recv.ptr_->template emplace<1>(static_cast<T&&>(value));
                recv.cont_.resume();
            }

            template <typename E>
            friend void tag_invoke(set_error_t, type&& recv, E&& error)
            {
                using err_t = std::decay_t<E>;
                if constexpr (std::is_same_v<err_t, std::exception_ptr>)
                    recv.ptr_->template emplace<2>(static_cast<E&&>(error));
                else if constexpr (std::is_same_v<err_t, std::error_code>)
                    recv.ptr_->template emplace<2>(std::make_exception_ptr(std::system_error(error)));
                else
                    recv.ptr_->template emplace<2>(std::make_exception_ptr(static_cast<E&&>(error)));
                recv.cont_.resume();
            }

            friend void tag_invoke(set_stopped_t, type&& recv)
            {
                recv.cont_.promise().unhandled_stopped().resume();
            }

            // @formatter:off
            template <typename Cpo, typename... Args> requires
                callable<Cpo, const P&, Args&&...> &&
                (!completion_cpo<Cpo>)
            friend auto tag_invoke(const Cpo cpo, type&& recv, Args&&... args)
                noexcept(nothrow_callable<Cpo, const P&, Args&&...>)
                -> call_result_t<Cpo, const P&, Args&&...>
            {
                return cpo(std::as_const(recv.cont_.promise()), static_cast<Args&&>(args)...);
            }
            // @formatter:on

        private:
            variant_t* ptr_ = nullptr;
            coro::coroutine_handle<P> cont_{};
        };

        template <typename S, typename P>
        using awaitable_receiver = typename awaitable_receiver_<S, P>::type;

        template <typename S, typename P>
        class sender_awaitable
        {
        public:
            using value_t = single_sender_value_type<S>;
            using result_t = with_regular_void_t<value_t>;
            using variant_t = std::variant<std::monostate, result_t, std::exception_ptr>;
            using recv_t = awaitable_receiver<S, P>;

            sender_awaitable(S&& snd, P& promise):
                state_(connect(
                    static_cast<S&&>(snd),
                    recv_t{ &result_, coro::coroutine_handle<P>::from_promise(promise) }
                )) {}

            bool await_ready() const noexcept { return false; }
            void await_suspend(coro::coroutine_handle<>) noexcept { start(state_); }

            value_t await_resume()
            {
                if (result_.index() == 2) // exception_ptr
                    std::rethrow_exception(std::get<2>(result_));
                if constexpr (std::is_void_v<value_t>) return;
                return static_cast<value_t&&>(std::get<1>(result_));
            }

        private:
            variant_t result_{};
            connect_result_t<S, recv_t> state_;
        };

        // @formatter:off
        template <typename S, typename P>
        concept awaitable_sender =
            single_typed_sender<S> &&
            sender_to<S, awaitable_receiver<S, P>> &&
            requires(P& promise) { { promise.unhandled_stopped() } -> std::convertible_to<coro::coroutine_handle<>>; };
        // @formatter:on
    }

    inline struct as_awaitable_t
    {
        template <typename T, typename P>
        constexpr decltype(auto) operator()(T&& value, P& promise) const
        {
            if constexpr (tag_invocable<as_awaitable_t, T, P&>)
                return clu::tag_invoke(*this, static_cast<T&&>(value), promise);
            else if constexpr (awaitable<T>)
                return static_cast<T&&>(value); // NOLINT(bugprone-branch-clone)
            else if constexpr (detail::awaitable_sender<T, P>)
                return detail::sender_awaitable<T, P>{ static_cast<T&&>(value), promise };
            else
                return static_cast<T&&>(value);
        }
    } constexpr as_awaitable{};

    // --- Schedulers ---

    // @formatter:off
    inline struct schedule_t
    {
        template <typename S> requires
            tag_invocable<schedule_t, S&&> &&
            sender<tag_invoke_result_t<schedule_t, S&&>>
        constexpr sender auto operator()(S&& sch) const
            noexcept(nothrow_tag_invocable<schedule_t, S&&>)
        {
            return clu::tag_invoke(*this, static_cast<S&&>(sch));
        }
    } constexpr schedule{};

    template <detail::completion_cpo Cpo>
    struct get_completion_scheduler_t;
    
    template <typename S>
    concept scheduler =
        std::copy_constructible<std::remove_cvref_t<S>> &&
        std::equality_comparable<std::remove_cvref_t<S>> &&
        requires(S&& sch, const get_completion_scheduler_t<set_value_t>& tag)
        {
            { exec::schedule(static_cast<S&&>(sch)) } -> sender;
            {
                tag_invoke(tag, exec::schedule(static_cast<S&&>(sch)))
            } -> std::same_as<std::remove_cvref_t<S>>;
        };

    template <detail::completion_cpo Cpo>
    struct get_completion_scheduler_t
    {
        template <sender S>
            requires tag_invocable<get_completion_scheduler_t, const S&>
        constexpr scheduler auto operator()(const S& snd) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<get_completion_scheduler_t, const S&> &&
                scheduler<tag_invoke_result_t<get_completion_scheduler_t, const S&>>,
                "get_completion_scheduler should be noexcept and return a scheduler");
            return clu::tag_invoke(*this, snd);
        }
    };
    // @formatter:on

    template <detail::completion_cpo Cpo>
    inline constexpr get_completion_scheduler_t<Cpo> get_completion_scheduler{};

    inline struct forwarding_scheduler_query_t
    {
        template <typename Cpo>
        constexpr bool operator()(Cpo&& cpo) noexcept
        {
            if constexpr (tag_invocable<forwarding_scheduler_query_t, Cpo>)
            {
                static_assert(
                    nothrow_tag_invocable<forwarding_scheduler_query_t, Cpo> &&
                    std::same_as<tag_invoke_result_t<forwarding_scheduler_query_t, Cpo>, bool>,
                    "forwarding_scheduler_query should be noexcept and return bool");
                return clu::tag_invoke(*this, static_cast<Cpo&&>(cpo));
            }
            return false;
        }
    } constexpr forwarding_scheduler_query{};

    enum class forward_progress_guarantee
    {
        concurrent,
        parallel,
        weakly_parallel
    };

    inline struct get_forward_progress_guarantee_t
    {
        template <scheduler S>
        constexpr forward_progress_guarantee operator()(const S& sch) noexcept
        {
            if constexpr (tag_invocable<get_forward_progress_guarantee_t, const S&>)
            {
                static_assert(
                    nothrow_tag_invocable<get_forward_progress_guarantee_t, const S&> &&
                    std::same_as<
                        tag_invoke_result_t<get_forward_progress_guarantee_t, const S&>,
                        forward_progress_guarantee
                    >,
                    "get_forward_progress_guarantee should be noexcept and return forward_progress_guarantee");
                return clu::tag_invoke(*this, sch);
            }
            return forward_progress_guarantee::weakly_parallel;
        }
    } constexpr get_forward_progress_guarantee{};

    // --- General Queries ---

    namespace detail
    {
        template <typename Tag>
        struct read_snd_
        {
            class type;
        };

        template <typename Tag>
        using read_snd = typename read_snd_<Tag>::type;

        template <typename Tag, typename R>
        struct read_ops_
        {
            class type;
        };

        template <typename Tag, typename R>
        class read_ops_<Tag, R>::type
        {
        public:
            template <typename R2>
            explicit type(R2&& recv): recv_(static_cast<R2&&>(recv)) {}

        private:
            R recv_;

            friend void tag_invoke(start_t, type& ops) noexcept
            {
                auto value = Tag{}(get_env(ops.recv_));
                exec::try_set_value(std::move(ops.recv_), std::move(value));
            }
        };

        template <typename Tag>
        class read_snd_<Tag>::type
        {
            template <typename R>
            using ops_t = typename read_ops_<Tag, std::decay_t<R>>::type;

            template <receiver R> requires
                callable<Tag, env_of_t<R>> &&
                receiver_of<R, call_result_t<Tag, env_of_t<R>>>
            friend ops_t<R> tag_invoke(connect_t, type, R&& recv)
            {
                return ops_t<R>(static_cast<R&&>(recv));
            }

            friend no_completion_signatures tag_invoke(get_completion_signatures_t, type, auto);

            template <typename Env>
                requires callable<Tag, Env>
            friend completion_signatures<
                set_value_t(call_result_t<Tag, Env>),
                set_error_t(std::exception_ptr)> tag_invoke(get_completion_signatures_t, type, Env);
        };
    }

    inline struct read_t
    {
        template <typename Tag>
        constexpr detail::read_snd<Tag> operator()(Tag) const noexcept { return {}; }
    } constexpr read{};

    inline struct get_scheduler_t
    {
        template <typename E>
            requires tag_invocable<get_scheduler_t, const E&>
        constexpr scheduler auto operator()(const E& env) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<get_scheduler_t, const E&> &&
                scheduler<tag_invoke_result_t<get_scheduler_t, const E&>> &&
                "get_scheduler should be noexcept and its return type should satisfy scheduler");
            return clu::tag_invoke(*this, env);
        }

        constexpr auto operator()() const noexcept { return read(*this); }
    } constexpr get_scheduler{};

    inline struct get_delegatee_scheduler_t
    {
        template <typename E>
            requires tag_invocable<get_delegatee_scheduler_t, const E&>
        constexpr scheduler auto operator()(const E& env) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<get_delegatee_scheduler_t, const E&> &&
                scheduler<tag_invoke_result_t<get_delegatee_scheduler_t, const E&>> &&
                "get_delegatee_scheduler should be noexcept and its return type should satisfy scheduler");
            return clu::tag_invoke(*this, env);
        }

        constexpr auto operator()() const noexcept { return read(*this); }
    } constexpr get_delegatee_scheduler{};

    inline struct get_allocator_t
    {
        template <typename E> requires
            nothrow_tag_invocable<get_allocator_t, const E&> // TODO: return type should model Allocator
        constexpr auto operator()(const E& env) const noexcept
        {
            static_assert(
                nothrow_tag_invocable<get_allocator_t, const E&>,
                "get_allocator should be noexcept");
            return clu::tag_invoke(*this, env);
        }

        constexpr auto operator()() const noexcept { return read(*this); }
    } constexpr get_allocator{};

    inline struct get_stop_token_t
    {
        template <typename E>
        constexpr stoppable_token auto operator()(const E& env) const noexcept
        {
            if constexpr (requires
            {
                requires nothrow_tag_invocable<get_stop_token_t, const E&>;
                requires stoppable_token<tag_invoke_result_t<get_stop_token_t, const E&>>;
            })
            {
                static_assert(
                    nothrow_tag_invocable<get_stop_token_t, const E&> &&
                    stoppable_token<tag_invoke_result_t<get_stop_token_t, const E&>>,
                    "get_stop_token should be noexcept and its return type should satisfy stoppable_token");
                return clu::tag_invoke(*this, env);
            }
            else
                return never_stop_token{};
        }

        constexpr auto operator()() const noexcept { return read(*this); }
    } constexpr get_stop_token{};

    template <typename R>
    using stop_token_of_t = std::remove_cvref_t<decltype(get_stop_token(std::declval<R>()))>;

    namespace detail
    {
        template <typename Cpo> concept fwd_schd_query = forwarding_scheduler_query(Cpo{});
        template <typename Cpo> concept fwd_snd_query = forwarding_sender_query(Cpo{});
        template <typename Cpo> concept fwd_recv_query = forwarding_receiver_query(Cpo{});
    }
}

namespace clu::this_thread
{
    inline struct execute_may_block_caller_t
    {
        template <exec::scheduler S>
        constexpr bool operator()(const S& sch) noexcept
        {
            if constexpr (
                tag_invocable<execute_may_block_caller_t, const S&>)
            {
                static_assert(
                    nothrow_tag_invocable<execute_may_block_caller_t, const S&> &&
                    std::same_as<tag_invoke_result_t<execute_may_block_caller_t, const S&>, bool>,
                    "execute_may_block_caller should be noexcept and return bool");
                return clu::tag_invoke(*this, sch);
            }
            return true;
        }
    } constexpr execute_may_block_caller{};
}
