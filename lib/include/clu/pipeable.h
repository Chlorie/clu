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
        struct bind_back_callable
        {
            Inv invocable;
            std::tuple<Args...> bound_args;

#define CLU_BIND_BACK_CALL_IMPL(cnst, ref)                                         \
            template <typename First>                                              \
                requires std::invocable<cnst Inv ref, First&&, Args...>            \
            constexpr decltype(auto) operator()(First&& first) cnst ref noexcept(  \
                std::is_nothrow_invocable_v<cnst Inv ref, First&&, Args...>)       \
            {                                                                      \
                return std::apply([&]<typename... As>(As&&... elems)               \
                {                                                                  \
                    return std::invoke(static_cast<cnst Inv ref>(invocable),       \
                        static_cast<First&&>(first), static_cast<As&&>(elems)...); \
                }, static_cast<cnst std::tuple<Args...> ref>(bound_args));         \
            }                                                                      \
            static_assert(true)

            CLU_BIND_BACK_CALL_IMPL(, &);
            CLU_BIND_BACK_CALL_IMPL(const, &);
            CLU_BIND_BACK_CALL_IMPL(, &&);
            CLU_BIND_BACK_CALL_IMPL(const, &&);

#undef CLU_BIND_BACK_CALL_IMPL
        };

        template <typename P, typename Q>
        struct compound_piper_callable
        {
            P first;
            Q second;

#define CLU_COMPOUND_PIPER_CALL_IMPL(cnst, ref)                                                     \
            template <typename First> requires                                                      \
                std::invocable<cnst P ref, First&&> &&                                              \
                std::invocable<cnst Q ref, std::invoke_result_t<cnst P ref, First&&>>               \
            constexpr decltype(auto) operator()(First&& first_arg) cnst ref noexcept(               \
                std::is_nothrow_invocable_v<cnst P ref, First&&> &&                                 \
                std::is_nothrow_invocable_v<cnst Q ref, std::invoke_result_t<cnst P ref, First&&>>) \
            {                                                                                       \
                return static_cast<cnst Q ref>(second)(                                             \
                    static_cast<cnst P ref>(first)(                                                 \
                        static_cast<First&&>(first_arg)));                                          \
            }                                                                                       \
            static_assert(true)

            CLU_COMPOUND_PIPER_CALL_IMPL(, &);
            CLU_COMPOUND_PIPER_CALL_IMPL(const, &);
            CLU_COMPOUND_PIPER_CALL_IMPL(, &&);
            CLU_COMPOUND_PIPER_CALL_IMPL(const, &&);

#undef CLU_COMPOUND_PIPER_CALL_IMPL
        };

        template <typename Call>
        struct piper : private Call
        {
            template <typename... Args>
            constexpr explicit piper(Args&&... args) noexcept(
                std::is_nothrow_constructible_v<Call, Args&&...>): Call(static_cast<Args&&>(args)...) {}

            using Call::operator();

            template <typename First, forwarding<piper> Self>
                requires std::invocable<Self, First&&>
            constexpr friend decltype(auto) operator|(First&& first, Self&& self) noexcept(
                std::is_nothrow_invocable_v<Self, First&&>)
            {
                return static_cast<Self&&>(self)(
                    static_cast<First&&>(first));
            }

            template <forwarding<piper> Self, typename Other>
                requires template_of<std::remove_cvref_t<Other>, piper>
            constexpr friend auto operator|(Self&& self, Other&& other) noexcept(
                std::is_nothrow_constructible_v<std::remove_cvref_t<Self>, Self&&> &&
                std::is_nothrow_constructible_v<std::remove_cvref_t<Other>, Other&&>)
            {
                using compound = compound_piper_callable<std::remove_cvref_t<Self>, std::remove_cvref_t<Other>>;
                return piper<compound>(static_cast<Self&&>(self), static_cast<Other&&>(other));
            }
        };

        template <typename... Args>
        using piper_of_t = piper<bind_back_callable<std::unwrap_ref_decay_t<Args>...>>;

    public:
        template <typename Inv2> requires std::convertible_to<Inv2&&, Inv>
        constexpr explicit pipeable(Inv2&& invocable) noexcept(std::is_nothrow_convertible_v<Inv2&&, Inv>):
            invocable_(static_cast<Inv2&&>(invocable)) {}

#define CLU_PIPEABLE_CALL_IMPL(cnst, ref)                                             \
        template <typename... Args>                                                   \
            requires std::invocable<cnst Inv ref, Args&&...>                          \
        constexpr decltype(auto) operator()(Args&&... args) cnst ref                  \
            noexcept(std::is_nothrow_invocable_v<cnst Inv ref, Args&&...>)            \
        {                                                                             \
            return std::invoke(                                                       \
                static_cast<cnst Inv ref>(invocable_), static_cast<Args&&>(args)...); \
        }                                                                             \
                                                                                      \
        template <typename... Args>                                                   \
        [[nodiscard]] constexpr auto operator()(Args&&... args) cnst ref              \
            noexcept(std::is_nothrow_constructible_v<                                 \
                piper_of_t<Args...>, cnst Inv ref, Args&&...>)                        \
        {                                                                             \
            return piper_of_t<Args...>(                                               \
                static_cast<cnst Inv ref>(invocable_), static_cast<Args&&>(args)...); \
        }                                                                             \
        static_assert(true)

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
