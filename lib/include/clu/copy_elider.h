#pragma once

#include "concepts.h"

namespace clu
{
    template <callable F>
    class copy_elider
    {
    public:
        // clang-format off
        template <decays_to<F> F2>
        constexpr explicit copy_elider(F2&& func) noexcept(
            std::is_nothrow_constructible_v<F, F2>):
            func_(static_cast<F2&&>(func)) {}
        // clang-format on

        using result_t = call_result_t<F>;

        constexpr explicit(false) operator result_t() noexcept(nothrow_callable<F>)
        {
            return static_cast<F&&>(func_)();
        }

    private:
        F func_;
    };

    template <typename F>
    copy_elider(F&&) -> copy_elider<std::decay_t<F>>;
} // namespace clu
