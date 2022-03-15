#pragma once

#include <utility>

namespace clu
{
    // clang-format off
    template <std::size_t begin, std::size_t end, typename F> requires
        (begin >= end) ||
        std::invocable<F, std::integral_constant<std::size_t, begin>>
    constexpr void static_for(F&& func) noexcept(
        begin >= end || std::is_nothrow_invocable_v<F, std::integral_constant<std::size_t, begin>>)
    // clang-format on
    {
        if constexpr (begin < end)
            [&]<std::size_t... i>(std::index_sequence<i...>)
            {
                (func(std::integral_constant<std::size_t, i + begin>{}), ...);
            }
        (std::make_index_sequence<end - begin>{});
    }

    template <std::size_t begin, std::size_t end, std::invocable<std::size_t> F>
        requires(!std::invocable<F, std::integral_constant<std::size_t, begin>>)
    constexpr void static_for(F&& func) noexcept(std::is_nothrow_invocable_v<F, std::size_t>)
    {
        if constexpr (begin < end)
            [&]<std::size_t... i>(std::index_sequence<i...>) { (func(i + begin), ...); }
        (std::make_index_sequence<end - begin>{});
    }
} // namespace clu
