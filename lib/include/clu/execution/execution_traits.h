#pragma once

#include <variant>

#include "awaitable_traits.h"
#include "../tag_invoke.h"
#include "../meta_list.h"
#include "../unique_coroutine_handle.h"
#include "../macros.h"

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
            return clu::tag_invoke(*this,
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
            return clu::tag_invoke(*this,
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
            return clu::tag_invoke(*this, static_cast<R&&>(recv));
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
        template <typename T>
        concept completion_cpo = same_as_any_of<T, set_value_t, set_error_t, set_done_t>;

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
            return clu::tag_invoke(*this, static_cast<O&&>(op));
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
                    requires callable<as_awaitable_t, A&&, promise_type&>
                auto await_transform(A&& a)
                    noexcept(nothrow_callable<as_awaitable_t, A&&, promise_type&>)
                    -> call_result_t<as_awaitable_t, A&&, promise_type&>
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

    template <typename S, receiver R>
    using connect_result_t = decltype(connect(std::declval<S>(), std::declval<R>()));

    // @formatter:off
    template <typename S, typename R>
    concept sender_to =
        sender<S> &&
        receiver<R> &&
        requires(S&& snd, R&& recv) { exec::connect(static_cast<S&&>(snd), static_cast<R&&>(recv)); };
    // @formatter:on

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

    namespace detail
    {
        template <typed_sender S, typename P>
        class sender_awaitable
        {
        private:
            using value_t = single_sender_value_type<S>;
            using result_t = with_regular_void_t<value_t>;
            using variant_t = std::variant<std::monostate, result_t, std::exception_ptr>;

        public:
            struct awaitable_receiver
            {
                variant_t* ptr_ = nullptr;
                std::coroutine_handle<P> cont_{};

                template <typename = int>
                    requires std::is_void_v<value_t>
                friend void tag_invoke(set_value_t, awaitable_receiver&& recv) noexcept
                {
                    recv.ptr_->template emplace<1>();
                    recv.cont_.resume();
                }

                template <forwarding<value_t> T = value_t>
                    requires (!std::is_void_v<value_t>)
                friend void tag_invoke(set_value_t, awaitable_receiver&& recv, T&& value)
                {
                    recv.ptr_->template emplace<1>(static_cast<T&&>(value));
                    recv.cont_.resume();
                }

                template <typename E>
                friend void tag_invoke(set_error_t, awaitable_receiver&& recv, E&& error)
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

                friend void tag_invoke(set_done_t, awaitable_receiver&& recv)
                {
                    recv.cont_.promise().unhandled_done().resume();
                }

                // @formatter:off
                template <typename Cpo, typename... Args> requires
                    callable<Cpo, const P&, Args&&...> &&
                    (!completion_cpo<Cpo>)
                friend auto tag_invoke(const Cpo cpo, awaitable_receiver&& recv, Args&&... args)
                    noexcept(nothrow_callable<Cpo, const P&, Args&&...>)
                    -> call_result_t<Cpo, const P&, Args&&...>
                {
                    return cpo(std::as_const(recv.cont_.promise()), static_cast<Args&&>(args)...);
                }
                // @formatter:on
            };

            sender_awaitable(S&& snd, P& promise):
                state_(connect(
                    static_cast<S&&>(snd),
                    awaitable_receiver{ &result_, std::coroutine_handle<P>::from_promise(promise) }
                )) {}

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<>) noexcept { start(state_); }

            value_t await_resume()
            {
                if (result_.index() == 2) // exception_ptr
                    std::rethrow_exception(std::get<2>(result_));
                if constexpr (std::is_void_v<value_t>) return;
                return static_cast<value_t&&>(std::get<1>(result_));
            }

        private:
            variant_t result_{};
            connect_result_t<S, awaitable_receiver> state_;
        };

        // @formatter:off
        template <typename S, typename P>
        concept awaitable_sender =
            single_typed_sender<S> &&
            sender_to<S, typename sender_awaitable<S, P>::awaitable_receiver> &&
            requires(P& promise) { { promise.unhandled_done() } -> std::convertible_to<std::coroutine_handle<>>; };
        // @formatter:on
    }

    inline struct as_awaitable_t
    {
        template <typename T, typename P>
        constexpr decltype(auto) operator()(T&& value, P& promise) const
        {
            if constexpr (tag_invocable<as_awaitable_t, T&&, P&>)
                return clu::tag_invoke(*this, static_cast<T&&>(value), promise);
            else if constexpr (awaitable<T>)
                return static_cast<T&&>(value); // NOLINT(bugprone-branch-clone)
            else if constexpr (detail::awaitable_sender<T, P>)
                return detail::sender_awaitable<T, P>{ static_cast<T&&>(value), promise };
            else
                return static_cast<T&&>(value);
        }
    } constexpr as_awaitable{};

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

    template <detail::completion_cpo Cpo>
    struct get_completion_scheduler_t
    {
        template <sender S> requires
            tag_invocable<get_completion_scheduler_t, const S&> &&
            scheduler<tag_invoke_result_t<get_completion_scheduler_t, const S&>>
        constexpr scheduler auto operator()(const S& snd) const
            noexcept(nothrow_tag_invocable<get_completion_scheduler_t, const S&>)
        {
            return clu::tag_invoke(*this, snd);
        }
    };
    // @formatter:on

    template <detail::completion_cpo Cpo>
    inline constexpr get_completion_scheduler_t<Cpo> get_completion_scheduler{};

    inline struct get_scheduler_t
    {
        template <receiver R> requires
            nothrow_tag_invocable<get_scheduler_t, const R&> &&
            scheduler<tag_invoke_result_t<get_scheduler_t, const R&>>
        constexpr scheduler auto operator()(const R& recv) const noexcept
        {
            return clu::tag_invoke(*this, recv);
        }
    } constexpr get_scheduler{};

    inline struct get_allocator_t
    {
        template <receiver R> requires
            nothrow_tag_invocable<get_allocator_t, const R&> // TODO: return type should model Allocator
        constexpr auto operator()(const R& recv) const noexcept
        {
            return clu::tag_invoke(*this, recv);
        }
    } constexpr get_allocator{};

    inline struct get_stop_token_t
    {
        template <receiver R>
        constexpr auto operator()(const R& recv) const noexcept
        {
            if constexpr (
                nothrow_tag_invocable<get_stop_token_t, const R&> &&
                stoppable_token<tag_invoke_result_t<get_stop_token_t, const R&>>
            )
                return clu::tag_invoke(*this, recv);
            else
                return never_stop_token{};
        }
    } constexpr get_stop_token{};

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
            if constexpr (
                nothrow_tag_invocable<get_forward_progress_guarantee_t, const S&> &&
                std::same_as<
                    tag_invoke_result_t<get_forward_progress_guarantee_t, const S&>,
                    forward_progress_guarantee
                >
            )
                return clu::tag_invoke(*this, sch);
            else
                return forward_progress_guarantee::weakly_parallel;
        }
    } constexpr get_forward_progress_guarantee{};

    template <typename R>
    using stop_token_of_t = std::remove_cvref_t<decltype(get_stop_token(std::declval<R>()))>;
}

namespace clu::this_thread
{
    inline struct execute_may_block_caller_t
    {
        template <exec::scheduler S>
        constexpr bool operator()(const S& sch) noexcept
        {
            if constexpr (
                nothrow_tag_invocable<execute_may_block_caller_t, const S&> &&
                std::same_as<tag_invoke_result_t<execute_may_block_caller_t, const S&>, bool>
            )
                return clu::tag_invoke(*this, sch);
            else
                return true;
        }
    } constexpr execute_may_block_caller{};
}

#include "../undef_macros.h"
