#pragma once

import clu.core;
#include "../coroutine.h"
#include "../macros.h"

namespace clu::exec
{
    namespace detail
    {
        template <typename T>
        concept valid_await_suspend_type = //
            same_as_any_of<T, void, bool> || //
            template_of<T, coro::coroutine_handle>;
    }

    // clang-format off
    template <typename A, typename P = void>
    concept awaiter = requires(A&& a, coro::coroutine_handle<P> h)
    {
        { a.await_ready() } -> boolean_testable;
        { a.await_suspend(h) } -> detail::valid_await_suspend_type;
        a.await_resume();
    };
    // clang-format on

    namespace detail::get_awt
    {
        template <typename A>
        constexpr static auto impl(A&& a, priority_tag<2>)
            CLU_SINGLE_RETURN_TRAILING(static_cast<A&&>(a).operator co_await());

        template <typename A>
        constexpr static auto impl(A&& a, priority_tag<1>)
            CLU_SINGLE_RETURN_TRAILING(operator co_await(static_cast<A&&>(a)));

        template <typename A>
        constexpr static auto impl(A&& a, priority_tag<0>) noexcept
        {
            return static_cast<A&&>(a);
        }

        struct get_awaiter_t
        {
            template <typename A>
            using awaiter_type = decltype(get_awt::impl(std::declval<A>(), priority_tag<2>{}));

            template <typename A>
            constexpr CLU_STATIC_CALL_OPERATOR(auto)(A&& a)
                CLU_SINGLE_RETURN_TRAILING(get_awt::impl(static_cast<A&&>(a), priority_tag<2>{}));
        };
    } // namespace detail::get_awt

    using detail::get_awt::get_awaiter_t;
    inline constexpr get_awaiter_t get_awaiter{};

    inline struct await_transform_t
    {
        template <typename P, typename A>
            requires requires(P& promise, A&& awaited) { promise.await_transform(static_cast<A&&>(awaited)); }
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(P& promise, A&& awaited)
            CLU_SINGLE_RETURN_TRAILING(promise.await_transform(static_cast<A&&>(awaited)));

        template <typename P, typename A>
        constexpr CLU_STATIC_CALL_OPERATOR(A)(P&, A&& awaited) noexcept
        {
            return static_cast<A&&>(awaited);
        }
    } constexpr await_transform{};

    namespace detail
    {
        // clang-format off
        template <typename A>
        concept unconditionally_awaitable = requires(A&& a)
        {
            { exec::get_awaiter(static_cast<A&&>(a)) } -> awaiter;
        };
        // clang-format on

        template <typename A, typename P>
        using await_transform_type =
            decltype(exec::await_transform(std::declval<with_regular_void_t<P>&>(), std::declval<A>()));
    } // namespace detail

    // clang-format off
    template <typename A, typename P = void>
    concept awaitable =
        requires { typename detail::await_transform_type<A, P>; } &&
        detail::unconditionally_awaitable<detail::await_transform_type<A, P>>;
    // clang-format on

    template <typename A, typename P = void>
    using awaiter_type_t = get_awaiter_t::awaiter_type<detail::await_transform_type<A, P>>;
    template <typename A, typename P = void>
    using await_result_t = decltype(std::declval<awaiter_type_t<A, P>>().await_resume());
} // namespace clu::exec
