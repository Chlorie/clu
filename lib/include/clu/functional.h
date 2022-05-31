#pragma once

#include "piper.h"

namespace clu
{
    namespace detail
    {
        // clang-format off
        template <typename F, typename T>
        concept invocable_with_single =
            (std::is_void_v<T> && std::invocable<F>) ||
            (!std::is_void_v<T> && std::invocable<F, T>);

        template <typename F, typename T>
        concept nothrow_invocable_with_single =
            (std::is_void_v<T> && nothrow_invocable<F>) ||
            (!std::is_void_v<T> && nothrow_invocable<F, T>);

        template <typename F, typename G, typename... Args>
        concept composed_invocable =
            std::invocable<F, Args...> &&
            invocable_with_single<G, std::invoke_result_t<F, Args...>>;

        template <typename F, typename G, typename... Args>
        concept nothrow_composed_invocable =
            nothrow_invocable<F, Args...> &&
            nothrow_invocable_with_single<G, std::invoke_result_t<F, Args...>>;
        // clang-format on

        template <typename F, typename G>
        class compose_fn
        {
        public:
            // clang-format off
            template <forwarding<F> F2, forwarding<G> G2>
            constexpr compose_fn(F2&& f, G2&& g) noexcept(
                std::is_nothrow_constructible_v<F, F2> && std::is_nothrow_constructible_v<G, G2>):
                f_(static_cast<F2&&>(f)), g_(static_cast<G2&&>(g)) {}
            // clang-format on

#if __cpp_explicit_this_parameter >= 202110L

            template <typename Self, typename... Args>
                requires composed_invocable<copy_cvref_t<Self, F>, copy_cvref_t<Self, G>, Args...>
            constexpr decltype(auto) operator()(this Self&& self, Args&&... args) noexcept(
                nothrow_composed_invocable<copy_cvref_t<Self, F>, copy_cvref_t<Self, G>, Args...>)
            {
                using cvref_f = copy_cvref_t<Self, F>;
                using cvref_g = copy_cvref_t<Self, G>;
                if constexpr (std::is_void_v<std::invoke_result_t<copy_cvref_t<Self, F>, Args...>>)
                {
                    std::invoke(static_cast<Self&&>(self).f_, static_cast<Args&&>(args)...);
                    return std::invoke(static_cast<Self&&>(self).g_);
                }
                else
                    return std::invoke(static_cast<Self&&>(self).g_,
                        std::invoke(static_cast<Self&&>(self).f_, static_cast<Args&&>(args)...));
            }

#else // ^^^ has deducing this / no deducing this vvv

#define CLU_COMPOSE_OP(Const, Ref)                                                                                     \
    template <typename... Args>                                                                                        \
        requires composed_invocable<Const F Ref, Const G Ref, Args...>                                                 \
    constexpr decltype(auto) operator()(Args&&... args)                                                                \
        Const Ref noexcept(nothrow_composed_invocable<Const F Ref, Const G Ref, Args...>)                              \
    {                                                                                                                  \
        if constexpr (std::is_void_v<std::invoke_result_t<Const F Ref, Args...>>)                                      \
        {                                                                                                              \
            std::invoke(static_cast<Const F Ref>(f_), static_cast<Args&&>(args)...);                                   \
            return std::invoke(static_cast<Const G Ref>(g_));                                                          \
        }                                                                                                              \
        else                                                                                                           \
            return std::invoke(static_cast<Const G Ref>(g_),                                                           \
                std::invoke(static_cast<Const F Ref>(f_), static_cast<Args&&>(args)...));                              \
    }                                                                                                                  \
    static_assert(true)

            CLU_COMPOSE_OP(, &);
            CLU_COMPOSE_OP(const, &);
            CLU_COMPOSE_OP(, &&);
            CLU_COMPOSE_OP(const, &&);
#undef CLU_COMPOSE_OP

#endif

        private:
            CLU_NO_UNIQUE_ADDRESS F f_;
            CLU_NO_UNIQUE_ADDRESS G g_;
        };
    } // namespace detail

    inline struct compose_t
    {
        template <typename F, typename G>
        constexpr auto operator()(F&& lhs, G&& rhs) const CLU_SINGLE_RETURN(
            detail::compose_fn<std::decay_t<F>, std::decay_t<G>>(static_cast<F&&>(lhs), static_cast<G&&>(rhs)));
        template <typename G>
        constexpr auto operator()(G&& rhs) const
            CLU_SINGLE_RETURN(clu::make_piper(clu::bind_back(*this, static_cast<G&&>(rhs))));
    } constexpr compose{};
} // namespace clu
