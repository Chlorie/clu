#pragma once

#include "../concepts.h"
#include "../coroutine.h"
#include "../macros.h"

namespace clu::exec
{
    // @formatter:off
    namespace detail
    {
        template <typename T>
        concept valid_await_suspend_type =
            same_as_any_of<T, void, bool> ||
            template_of<T, coro::coroutine_handle>;
    }

    template <typename A, typename P = void>
    concept awaiter =
        requires(A&& a, coro::coroutine_handle<P> h)
        {
            { a.await_ready() } -> boolean_testable;
            { a.await_suspend(h) } -> detail::valid_await_suspend_type;
            a.await_resume();
        };
    // @formatter:on

    namespace detail::get_awt
    {
        template <typename A>
        constexpr static auto impl(A&& a, priority_tag<2>)
        CLU_SINGLE_RETURN(static_cast<A&&>(a).operator co_await());

        template <typename A>
        constexpr static auto impl(A&& a, priority_tag<1>)
        CLU_SINGLE_RETURN(operator co_await(static_cast<A&&>(a)));

        template <typename A>
        constexpr static decltype(auto) impl(A&& a, priority_tag<0>) noexcept { return static_cast<A&&>(a); }

        struct get_awaiter_t
        {
            template <typename A>
            using awaiter_type = decltype(get_awt::impl(std::declval<A>(), priority_tag<2>{}));

            template <typename A> requires awaiter<awaiter_type<A>>
            constexpr auto operator()(A&& a) const
            CLU_SINGLE_RETURN(get_awt::impl(static_cast<A&&>(a), priority_tag<2>{}));
        };
    }

    using detail::get_awt::get_awaiter_t;
    inline constexpr get_awaiter_t get_awaiter{};

    inline struct await_transform_t
    {
        template <typename P, typename A>
            requires requires(P& promise, A&& awaited) { promise.await_transform(static_cast<A&&>(awaited)); }
        constexpr auto operator()(P& promise, A&& awaited) const
        CLU_SINGLE_RETURN(promise.await_transform(static_cast<A&&>(awaited)));

        template <typename P, typename A>
        constexpr A operator()(P&, A&& awaited) const noexcept
        {
            return static_cast<A&&>(awaited);
        }
    } constexpr await_transform{};

    namespace detail
    {
        template <typename A>
        concept unconditionally_awaitable = requires(A&& a) { exec::get_awaiter(static_cast<A&&>(a)); };

        template <typename A, typename P>
        using await_transform_type = decltype(exec::await_transform(
            std::declval<with_regular_void_t<P>&>(),
            std::declval<A>()
        ));
    }

    // @formatter:off
    template <typename A, typename P = void>
    concept awaitable =
        requires { typename detail::await_transform_type<A, P>; } &&
        detail::unconditionally_awaitable<detail::await_transform_type<A, P>>;
    // @formatter:on

    template <typename A, typename P = void>
    using awaiter_type_t = get_awaiter_t::awaiter_type<detail::await_transform_type<A, P>>;
    template <typename A, typename P = void>
    using await_result_t = decltype(std::declval<awaiter_type_t<A, P>>().await_resume());
}

#include "../undef_macros.h"
