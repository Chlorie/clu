// Pipe operator adapter utility
// Adapted from P2387R3: Pipe support for user-defined range adaptors

#pragma once

#include "concepts.h"
#include "invocable_wrapper.h"

namespace clu
{
    template <typename Inv>
    constexpr auto make_piper(Inv&& invocable) noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<Inv>, Inv&&>);

    class piper_mixin
    {
        template <typename Arg, typename Self>
            requires std::invocable<Self, Arg&&>
        constexpr friend decltype(auto) operator|(Arg&& first, Self&& self) noexcept(
            std::is_nothrow_invocable_v<Self, Arg&&>)
        {
            return static_cast<Self&&>(self)(static_cast<Arg&&>(first));
        }

        template <typename Other, typename Self>
            requires std::derived_from<std::remove_cvref_t<Other>, piper_mixin>
        constexpr friend auto operator|(Other&& other, Self&& self) noexcept(
            std::is_nothrow_constructible_v<std::remove_cvref_t<Other>, Other&&>&&
                std::is_nothrow_constructible_v<std::remove_cvref_t<Self>, Self&&>)
        {
            using compound = compound_callable<std::remove_cvref_t<Other>, std::remove_cvref_t<Self>>;
            return clu::make_piper(compound{static_cast<Other&&>(other), static_cast<Self&&>(self)});
        }

        template <typename First, typename Second>
        struct compound_callable
        {
#define CLU_COMPOUND_PIPER_CALL_IMPL(cnst, ref)                                                                        \
    template <typename Arg>                                                                                            \
        requires std::invocable<cnst First ref, Arg&&> &&                                                              \
            std::invocable<cnst Second ref, std::invoke_result_t<cnst First ref, Arg&&>>                               \
    constexpr decltype(auto) operator()(Arg&& arg)                                                                     \
        cnst ref noexcept(std::is_nothrow_invocable_v<cnst First ref, Arg&&> &&                                        \
            std::is_nothrow_invocable_v<cnst Second ref, std::invoke_result_t<cnst First ref, Arg&&>>)                 \
    {                                                                                                                  \
        return static_cast<cnst First ref>(first)(static_cast<cnst Second ref>(second)(static_cast<Arg&&>(arg)));      \
    }                                                                                                                  \
    static_assert(true)

            CLU_COMPOUND_PIPER_CALL_IMPL(, &);
            CLU_COMPOUND_PIPER_CALL_IMPL(const, &);
            CLU_COMPOUND_PIPER_CALL_IMPL(, &&);
            CLU_COMPOUND_PIPER_CALL_IMPL(const, &&);

#undef CLU_COMPOUND_PIPER_CALL_IMPL

            [[no_unique_address]] First first;
            [[no_unique_address]] Second second;
        };
    };

    namespace detail
    {
        template <typename Inv, typename... Args>
        class bind_back_t
        {
        public:
            template <typename Inv2, typename... Args2>
            constexpr explicit bind_back_t(Inv2&& inv, Args2&&... args) noexcept(
                std::is_nothrow_constructible_v<Inv, Inv2&&> &&
                (std::is_nothrow_constructible_v<Args, Args2&&> && ...)):
                invocable_(static_cast<Inv2&&>(inv)),
                args_(static_cast<Args2&&>(args)...)
            {
            }

#define CLU_BIND_BACK_CALL_IMPL(cnst, ref)                                                                             \
    template <typename First>                                                                                          \
        requires std::invocable<cnst Inv ref, First&&, Args...>                                                        \
    constexpr decltype(auto) operator()(First&& first)                                                                 \
        cnst ref noexcept(std::is_nothrow_invocable_v<cnst Inv ref, First&&, Args...>)                                 \
    {                                                                                                                  \
        return std::apply(                                                                                             \
            [&]<typename... As>(As && ... elems) {                                                                     \
                return std::invoke(                                                                                    \
                    static_cast<cnst Inv ref>(invocable_), static_cast<First&&>(first), static_cast<As&&>(elems)...);  \
            },                                                                                                         \
            static_cast<cnst std::tuple<Args...> ref>(args_));                                                         \
    }                                                                                                                  \
    static_assert(true)

            CLU_BIND_BACK_CALL_IMPL(, &);
            CLU_BIND_BACK_CALL_IMPL(const, &);
            CLU_BIND_BACK_CALL_IMPL(, &&);
            CLU_BIND_BACK_CALL_IMPL(const, &&);

#undef CLU_BIND_BACK_CALL_IMPL

        private:
            [[no_unique_address]] Inv invocable_;
            std::tuple<Args...> args_;
        };
    } // namespace detail

    template <typename Inv, typename... Args>
    constexpr auto bind_back(Inv&& invocable, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<Inv, Inv&&> && (std::is_nothrow_constructible_v<Args, Args&&> && ...))
    {
        return detail::bind_back_t<std::decay_t<Inv>, std::decay_t<Args>...>(
            static_cast<Inv&&>(invocable), static_cast<Args&&>(args)...);
    }

    // @formatter:off
    template <typename Inv>
    constexpr auto make_piper(Inv&& invocable) noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<Inv>, Inv&&>)
    {
        using inv_t = std::remove_cvref_t<Inv>;
        struct pipeable : private inv_t, piper_mixin
        {
            constexpr explicit pipeable(Inv&& inv) noexcept(std::is_nothrow_constructible_v<inv_t, Inv&&>):
                inv_t(static_cast<Inv&&>(inv))
            {
            }
            using inv_t::operator();
        };
        return pipeable(static_cast<Inv&&>(invocable));
    }
    // @formatter:on
} // namespace clu
