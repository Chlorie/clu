#pragma once

#include <concepts>
#include <coroutine>
#include <exception>

#include "clu/concepts.h"

namespace clu
{
    namespace detail
    {
        template <typename T>
        concept valid_await_suspend_type = std::same_as<T, void> || std::same_as<T, bool> || template_of<T, std::coroutine_handle>;

        template <typename T>
        concept half_awaiter = requires(T&& awt, std::coroutine_handle<> hdl)
        {
            { awt.await_ready() ? (void)0 : (void)0 }; // NOLINT(bugprone-branch-clone)
            { awt.await_suspend(hdl) } -> valid_await_suspend_type;
        };
    }

    template <typename T>
    concept awaiter = detail::half_awaiter<T> && requires(T&& awt)
    {
        { awt.await_resume() };
    };

    template <typename T, typename Res>
    concept awaiter_of = detail::half_awaiter<T> && requires(T&& awt)
    {
        { awt.await_resume() } -> std::convertible_to<Res>;
    };

#define CLU_SINGLE_RETURN(expr) noexcept(noexcept(expr)) -> decltype(expr) { return expr; }
    namespace detail
    {
        struct none_adl {};
        void operator co_await(none_adl); // Hide non-ADL version of operator co_await

        template <size_t I> struct priority_tag : priority_tag<I - 1> {};
        template <> struct priority_tag<0> {};

        template <typename T>
        auto get_awaiter_impl(T&& arg, priority_tag<2>)
        CLU_SINGLE_RETURN(static_cast<T&&>(arg).operator co_await());

        template <typename T>
        auto get_awaiter_impl(T&& arg, priority_tag<1>)
        CLU_SINGLE_RETURN(operator co_await(static_cast<T&&>(arg)));

        template <typename T>
        decltype(auto) get_awaiter_impl(T&& arg, priority_tag<0>) noexcept { return static_cast<T&&>(arg); }
    }

    template <typename T>
    auto get_awaiter(T&& awt)
    CLU_SINGLE_RETURN(detail::get_awaiter_impl(static_cast<T&&>(awt), detail::priority_tag<2>{}));
#undef CLU_SINGLE_RETURN

    template <typename T> using awaiter_type_t = decltype(get_awaiter(std::declval<T>()));
    template <typename T> struct awaiter_type
    {
        using type = awaiter_type_t<T>;
    };

    template <typename T> concept awaitable = awaiter<awaiter_type_t<T>>;
    template <typename T, typename Res> concept awaitable_of = awaiter_of<awaiter_type_t<T>, Res>;

    template <typename T> using await_result_t = decltype(std::declval<awaiter_type_t<T>>().await_resume());
    template <typename T> struct await_result
    {
        using type = await_result_t<T>;
    };

    template <typename T, typename E = std::exception_ptr>
    concept receiver = std::move_constructible<T> && requires(T recv, E error)
    {
        &T::operator();
        { recv.set_error(error) } -> std::same_as<void>;
        { recv.set_done() } -> std::same_as<void>;
    };

    template <typename T, typename... V>
    concept receiver_of = receiver<T> && requires(T recv, V ... args)
    {
        { recv(args...) } -> std::same_as<void>;
    };
}
