#pragma once

#include <variant>

#include "awaitable_traits.h"
#include "../tag_invoke.h"
#include "../meta_list.h"
#include "../macros.h"
#include "../unique_coroutine_handle.h"

namespace clu::exec
{
    // @formatter:off
    inline struct set_value_t
    {
        template <typename R, typename... Vs>
            requires tag_invocable<set_value_t, R&&, Vs&&...>
        constexpr auto operator()(R&& recv, Vs&&... values) const
            noexcept(nothrow_tag_invocable<set_value_t, R&&, Vs&&...>)
            -> tag_invoke_result_t<set_value_t, R&&, Vs&&...>
        {
            return tag_invoke(*this,
                static_cast<R&&>(recv), static_cast<Vs&&>(values)...);
        }
    } constexpr set_value{};

    inline struct set_error_t
    {
        template <typename R, typename E>
            requires tag_invocable<set_error_t, R&&, E&&>
        constexpr auto operator()(R&& recv, E&& error) const
            noexcept(nothrow_tag_invocable<set_error_t, R&&, E&&>)
            -> tag_invoke_result_t<set_error_t, R&&, E&&>
        {
            return tag_invoke(*this,
                static_cast<R&&>(recv), static_cast<E&&>(error));
        }
    } constexpr set_error{};

    inline struct set_done_t
    {
        template <typename R>
            requires tag_invocable<set_done_t, R&&>
        constexpr auto operator()(R&& recv) const
            noexcept(nothrow_tag_invocable<set_done_t, R&&>)
            -> tag_invoke_result_t<set_done_t, R&&>
        {
            return tag_invoke(*this, static_cast<R&&>(recv));
        }
    } constexpr set_done{};

    template <typename R, typename E = std::exception_ptr>
    concept receiver =
        std::move_constructible<std::remove_cvref_t<R>> &&
        std::constructible_from<std::remove_cvref_t<R>, R> &&
        requires(std::remove_cvref_t<R>&& recv, E&& error)
        {
            { exec::set_done(std::move(recv)) } noexcept;
            { exec::set_error(std::move(recv), static_cast<E&&>(error)) } noexcept;
        };

    template <typename R, typename... Vs>
    concept receiver_of =
        receiver<R> &&
        requires(std::remove_cvref_t<R>&& recv, Vs&&... values)
        {
            exec::set_value(std::move(recv), static_cast<Vs>(values)...);
        };

    namespace detail
    {
        template <template <template <typename...> typename, template <typename...> typename> typename V>
        struct has_value_types {};
        template <template <template <typename...> typename> typename E>
        struct has_error_types {};

        template <typename S>
        concept has_sender_types =
            requires
            {
                typename has_value_types<S::template value_types>;
                typename has_error_types<S::template error_types>;
                typename std::bool_constant<S::sends_done>;
            };
    }

    inline struct start_t
    {
        template <typename O>
            requires tag_invocable<start_t, O&&>
        constexpr auto operator()(O&& op) const
            noexcept(nothrow_tag_invocable<start_t, O&&>)
            -> tag_invoke_result_t<start_t, O&&>
        {
            return tag_invoke(*this, static_cast<O&&>(op));
        }
    } constexpr start{};

    template <typename O>
    concept operation_state =
        std::destructible<O> &&
        std::is_object_v<O> &&
        requires(O& op) { { exec::start(op) } noexcept; };
    // @formatter:on

    struct sender_base {};

    namespace detail
    {
        // all false
        template <typename S, bool HasSenderTypes, bool DerivedFromSenderBase, bool IsAwaitable>
        struct sender_traits_base
        {
            using unspecialized_ = void;
        };

        // has sender types
        template <typename S, bool D, bool I>
        struct sender_traits_base<S, true, D, I>
        {
            template <template <typename...> typename Tuple, template <typename...> typename Variant>
            using value_types = typename S::template value_types<Tuple, Variant>;

            template <template <typename...> typename Variant>
            using error_types = typename S::template error_types<Variant>;

            static constexpr bool sends_done = S::sends_done;
        };

        // derived from sender_base
        template <typename S, bool I>
        struct sender_traits_base<S, false, true, I> {};

        // is awaitable
        template <typename S>
        struct sender_traits_base<S, false, false, true>
        {
            template <template <typename...> typename Tuple, template <typename...> typename Variant>
            using value_types = Variant<std::conditional_t<
                std::is_void_v<await_result_t<S>>,
                Tuple<>,
                Tuple<with_regular_void_t<await_result_t<S>>>
            >>;

            template <template <typename...> typename Variant>
            using error_types = Variant<std::exception_ptr>;

            static constexpr bool sends_done = false;
        };
    }

    template <typename S>
    struct sender_traits :
        detail::sender_traits_base<
            S,
            detail::has_sender_types<S>,
            std::derived_from<S, sender_base>,
            awaitable<S>
        > {};

    // @formatter:off
    template <typename S>
    concept sender =
        std::move_constructible<std::remove_cvref_t<S>> &&
        !requires { typename sender_traits<std::remove_cvref_t<S>>::unspecialized_; };

    template <typename S>
    concept typed_sender =
        sender<S> &&
        detail::has_sender_types<sender_traits<std::remove_cvref_t<S>>>;

    template <typename S, typename... Ts>
    concept sender_of =
        typed_sender<S> &&
        std::same_as<
            type_list<Ts...>,
            typename sender_traits<S>::template value_types<type_list, std::type_identity_t>
        >;
    // @formatter:on

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

    // @formatter:off
    template <
        typed_sender S,
        template <typename...> typename Tuple = std::tuple,
        template <typename...> typename Variant = variant_or_empty
    >
    using value_types_of_t =
        typename sender_traits<std::remove_cvref_t<S>>::template value_types<Tuple, Variant>;

    template <
        typed_sender S,
        template <typename...> typename Variant = variant_or_empty
    >
    using error_types_of_t =
        typename sender_traits<std::remove_cvref_t<S>>::template error_types<Variant>;
    // @formatter:on

    struct as_awaitable_t;
    extern const as_awaitable_t as_awaitable;

    inline struct connect_t
    {
    private:
        template <typename R>
        class operation_state_task
        {
        public:
            struct promise_type
            {
                R& recv;

                template <typename S> promise_type(S&&, R& recv): recv(recv) {}

                std::suspend_always initial_suspend() const noexcept { return {}; }
                [[noreturn]] std::suspend_always final_suspend() const noexcept { std::terminate(); }
                [[noreturn]] void unhandled_exception() const noexcept { std::terminate(); }
                [[noreturn]] void return_void() const noexcept { std::terminate(); }

                auto unhandled_done() const
                {
                    exec::set_done(static_cast<R&&>(recv));
                    return std::noop_coroutine();
                }

                template <typename A>
                A&& await_transform(A&& a) const noexcept { return static_cast<A&&>(a); }

                // @formatter:off
                template <typename A>
                    requires tag_invocable<as_awaitable_t, A&&, promise_type&>
                auto await_transform(A&& a)
                    noexcept(nothrow_tag_invocable<as_awaitable_t, A&&, promise_type&>)
                    -> tag_invoke_result_t<as_awaitable_t, A&&, promise_type&>
                {
                    return clu::tag_invoke(as_awaitable,
                        static_cast<A&&>(a), *this);
                }

                template <typename Cpo, typename... Args>
                    requires tag_invocable<Cpo, const R&, Args&&...>
                auto tag_invoke(Cpo, Args&&... args)
                    noexcept(nothrow_tag_invocable<Cpo, const R&, Args&&...>)
                    -> tag_invoke_result_t<Cpo, const R&, Args&&...>
                {
                    return clu::tag_invoke(Cpo{},
                        recv, static_cast<Args&&>(args)...);
                }
                // @formatter:on

                template <typename F>
                auto yield_value(F&& func) const
                {
                    struct suspend_call
                    {
                        F&& func;
                        bool await_ready() const noexcept { return false; }
                        void await_suspend() const noexcept { static_cast<F&&>(func)(); }
                        [[noreturn]] void await_resume() const noexcept { std::terminate(); }
                    };
                    return suspend_call{ static_cast<F&&>(func) };
                }

                auto get_return_object() noexcept
                {
                    const auto handle = std::coroutine_handle<promise_type>::from_promise(*this);
                    return operation_state_task(handle);
                }
            };

            explicit operation_state_task(const std::coroutine_handle<> handle): handle_(handle) {}
            friend void tag_invoke(start_t, operation_state_task& self) noexcept { self.handle_.get().resume(); }
        private:
            unique_coroutine_handle<> handle_{};
        };

        template <typename S, typename R>
        using await_result_type = await_result_t<
            std::decay_t<S>,
            typename operation_state_task<std::decay_t<R>>::promise_type
        >;

        template <typename S, typename R>
        constexpr static operation_state_task<R> connect_awaitable(S snd, R recv)
        {
            using res_t = await_result_type<S, R>;
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
        template <sender S, receiver R> requires
            tag_invocable<connect_t, S&&, R&&> &&
            operation_state<tag_invoke_result_t<connect_t, S&&, R&&>>
        constexpr static auto select(S&& snd, R&& recv, priority_tag<1>)
            noexcept(nothrow_tag_invocable<connect_t, S&&, R&&>)
            -> tag_invoke_result_t<connect_t, S&&, R&&>
        {
            return tag_invoke(connect_t{},
                static_cast<S&&>(snd), static_cast<R&&>(recv));
        }
        // @formatter:on

        template <typename S, typename R> requires
            awaitable<S, typename operation_state_task<std::decay_t<R>>::promise_type> &&
            (
                (std::is_void_v<await_result_type<S, R>> && receiver_of<R>) ||
                receiver_of<R, await_result_type<S, R>>
            )
        constexpr static auto select(S&& snd, R&& recv, priority_tag<0>)
        {
            return connect_t::connect_awaitable(snd, recv);
        }

        template <typename S, typename R>
        using result_type = decltype(connect_t::select(std::declval<S>(), std::declval<R>(), priority_tag<1>{}));

    public:
        template <typename S, receiver R>
        constexpr auto operator()(S&& snd, R&& recv) const
        CLU_SINGLE_RETURN(connect_t::select(static_cast<S&&>(snd), static_cast<R&&>(recv), priority_tag<1>{}));
    } constexpr connect{};

    namespace detail
    {
        template <typed_sender S>
        constexpr auto ssvt_impl() noexcept
        {
            using types = typename sender_traits<S>::template value_types<type_list, type_list>;
            constexpr auto overloads = types::size;
            if constexpr (types::size == 0)
                return type_tag<void>;
            else if constexpr (types::size == 1)
            {
                using tuple = typename types::template apply<single_type_t>;
                if constexpr (tuple::size == 0)
                    return type_tag<void>;
                else
                    return type_tag<typename tuple::template apply<single_type_t>>;
            }
        }
    }

    template <typed_sender S>
    using single_sender_value_type = decltype(detail::ssvt_impl<S>());

    // @formatter:off
    template <typename S>
    concept single_typed_sender =
        typed_sender<S> &&
        requires { typename single_sender_value_type<S>; };
    // @formatter:on

    template <typed_sender S>
    class sender_awaitable
    {
    private:
        using value_t = single_sender_value_type<S>;
        using result_t = with_regular_void_t<value_t>;

    public:
        struct awaitable_receiver { };

    private:
        std::variant<std::monostate, result_t, std::exception_ptr> result_{};
    };

    inline struct as_awaitable_t
    {
        // TODO
    } constexpr as_awaitable{};

    inline struct schedule_t
    {
        // TODO   
    } constexpr schedule{};

    inline struct get_scheduler_t
    {
        // TODO    
    } constexpr get_scheduler{};

    inline struct get_allocator_t
    {
        // TODO
    } constexpr get_allocator{};

    inline struct get_stop_token_t
    {
        // TODO
    } constexpr get_stop_token{};

    inline struct get_completion_scheduler_t
    {
        // TODO
    } constexpr get_completion_scheduler{};

    // @formatter:off
    template <typename S>
    concept scheduler =
        std::copy_constructible<std::remove_cvref_t<S>> &&
        std::equality_comparable<std::remove_cvref_t<S>> &&
        requires(S&& sch)
        {
            { exec::schedule(static_cast<S&&>(sch)) } -> sender_of;
            {
                tag_invoke(
                    get_completion_scheduler_t<set_value_t>{},
                    exec::schedule(static_cast<S&&>(sch))
                )
            } -> std::same_as<std::remove_cvref_t<S>>;
        };
    // @formatter:on

    enum class forward_progress_guarantee
    {
        concurrent,
        parallel,
        weakly_parallel
    };

    inline struct get_forward_progress_guarantee_t
    {
        // TODO
    } constexpr get_forward_progress_guarantee{};

    template <typename R>
    using stop_token_of_t = std::remove_cvref_t<decltype(get_stop_token(std::declval<R>()))>;
}

namespace clu::this_thread
{
    inline struct execute_may_block_caller_t
    {
        // TODO
    } constexpr execute_may_block_caller{};
}

#include "../undef_macros.h"
