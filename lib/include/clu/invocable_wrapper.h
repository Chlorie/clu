#pragma once

#include <functional>

#include "concepts.h"
#include "function_traits.h"

namespace clu
{
    template <typename Inv>
    class invocable_wrapper;

    // clang-format off
    template <typename Inv> requires
        (!std::is_final_v<Inv>) &&
        std::is_class_v<Inv>
    class invocable_wrapper<Inv> : public Inv
    // clang-format on
    {
    public:
        using Inv::operator();

        template <forwarding<Inv> Inv2 = Inv>
        constexpr explicit invocable_wrapper(Inv2&& inv) noexcept(std::is_nothrow_constructible_v<Inv, Inv2&&>):
            Inv(static_cast<Inv2>(inv))
        {
        }
    };

    template <typename Inv>
        requires std::is_final_v<Inv> || std::is_member_pointer_v<Inv>
    class invocable_wrapper<Inv>
    {
    public:
        constexpr ~invocable_wrapper() noexcept = default;
        constexpr invocable_wrapper(const invocable_wrapper&) = default;
        constexpr invocable_wrapper(invocable_wrapper&&) noexcept = default;
        constexpr invocable_wrapper& operator=(const invocable_wrapper&) = default;
        constexpr invocable_wrapper& operator=(invocable_wrapper&&) noexcept = default;

        template <forwarding<Inv> Inv2 = Inv>
        constexpr explicit invocable_wrapper(Inv2&& inv) noexcept(std::is_nothrow_constructible_v<Inv, Inv2&&>):
            invocable_(static_cast<Inv2>(inv))
        {
        }

#define CLU_FN_WRAPPER_CALL_DEF(cnst, ref)                                                                             \
    template <typename... Args>                                                                                        \
        requires std::invocable<cnst Inv ref, Args&&...>                                                               \
    constexpr auto operator()(Args&&... args) cnst ref noexcept(std::is_nothrow_invocable_v<cnst Inv ref, Args&&...>)  \
        ->std::invoke_result_t<cnst Inv ref, Args&&...>                                                                \
    {                                                                                                                  \
        return std::invoke(static_cast<cnst Inv ref>(invocable_), static_cast<Args>(args)...);                         \
    }                                                                                                                  \
    static_assert(true)

        CLU_FN_WRAPPER_CALL_DEF(, &);
        CLU_FN_WRAPPER_CALL_DEF(, &&);
        CLU_FN_WRAPPER_CALL_DEF(const, &);
        CLU_FN_WRAPPER_CALL_DEF(const, &&);
#undef CLU_FN_WRAPPER_CALL_DEF

    private:
        CLU_NO_UNIQUE_ADDRESS Inv invocable_;
    };

    template <typename Res, typename... Args>
    class invocable_wrapper<Res (*)(Args...)>
    {
    public:
        constexpr explicit(false) invocable_wrapper(Res (*fptr)(Args...)) noexcept: fptr_(fptr) {}
        constexpr Res operator()(Args... args) const { return fptr_(static_cast<Args>(args)...); }

    private:
        Res (*fptr_)(Args...) = nullptr;
    };

    template <typename Res, typename... Args>
    class invocable_wrapper<Res (*)(Args...) noexcept>
    {
    public:
        constexpr explicit(false) invocable_wrapper(Res (*fptr)(Args...) noexcept) noexcept: fptr_(fptr) {}
        constexpr Res operator()(Args... args) const noexcept { return fptr_(static_cast<Args>(args)...); }

    private:
        Res (*fptr_)(Args...) noexcept = nullptr;
    };

    template <typename Inv>
    invocable_wrapper(Inv) -> invocable_wrapper<std::decay_t<Inv>>;
} // namespace clu
