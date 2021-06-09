// Pipe operator adapter utility
// Roughly as specified in P2281R1: Clarifying Range Adaptor Objects

#pragma once

#include <functional>

#include "type_traits.h"
#include "concepts.h"

namespace clu
{
    template <typename Inv>
    class pipeable
    {
    private:
        Inv invocable_;

        template <typename... Args>
        class bind_back_t
        {
        private:
            Inv invocable_;
            std::tuple<Args...> bound_args_;

        public:
            template <typename Inv2, typename... Args2>
            constexpr explicit bind_back_t(Inv2&& invocable, Args2&&... bound_args) noexcept(
                std::is_nothrow_convertible_v<Inv2&&, Inv> && (std::is_nothrow_convertible_v<Args2&&, Args> && ...)):
                invocable_(static_cast<Inv2&&>(invocable)), bound_args_(static_cast<Args2&&>(bound_args)...) {}

            template <typename First, forwarding<bind_back_t> Self>
                requires std::invocable<copy_cvref_t<Self&&, Inv>, First&&, Args...>
            constexpr friend decltype(auto) operator|(First&& first, Self&& self) noexcept(
                std::is_nothrow_invocable_v<copy_cvref_t<Self&&, Inv>, First&&, Args...>)
            {
                return std::apply([&]<typename... As>(As&&... elems)
                {
                    return std::invoke(static_cast<Self&&>(self).invocable_,
                        static_cast<First&&>(first), static_cast<As&&>(elems)...);
                }, static_cast<Self&&>(self).bound_args_);
            }
        };

    public:
        template <typename Inv2> requires std::convertible_to<Inv2&&, Inv>
        constexpr explicit pipeable(Inv2&& invocable) noexcept(std::is_nothrow_convertible_v<Inv2&&, Inv>):
            invocable_(static_cast<Inv2&&>(invocable)) {}

#define CLU_PIPEABLE_CALL_IMPL(cnst, ref)                                                \
        template <typename... Args>                                                      \
            requires std::invocable<cnst Inv ref, Args&&...>                             \
        constexpr decltype(auto) operator()(Args&&... args) cnst ref                     \
            noexcept(std::is_nothrow_invocable_v<cnst Inv ref, Args&&...>)               \
        {                                                                                \
            return std::invoke(                                                          \
                static_cast<cnst Inv ref>(invocable_), static_cast<Args&&>(args)...);    \
        }                                                                                \
                                                                                         \
        template <typename... Args>                                                      \
        [[nodiscard]] constexpr auto operator()(Args&&... args) cnst ref                 \
            noexcept(std::is_nothrow_constructible_v<                                    \
                bind_back_t<std::unwrap_ref_decay_t<Args>...>, cnst Inv ref, Args&&...>) \
        {                                                                                \
            return bind_back_t<std::unwrap_ref_decay_t<Args>...>(                        \
                static_cast<cnst Inv ref>(invocable_), static_cast<Args&&>(args)...);    \
        }

        CLU_PIPEABLE_CALL_IMPL(, &);
        CLU_PIPEABLE_CALL_IMPL(const, &);
        CLU_PIPEABLE_CALL_IMPL(, &&);
        CLU_PIPEABLE_CALL_IMPL(const, &&);

#undef CLU_PIPEABLE_CALL_IMPL
    };

    template <typename Inv> pipeable(Inv) -> pipeable<Inv>;

    template <typename Inv>
    [[nodiscard]] constexpr auto make_pipeable(Inv&& invocable) { return pipeable(static_cast<Inv&&>(invocable)); }
}
