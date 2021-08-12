#pragma once

#include <utility>

namespace clu
{
    template <size_t begin, size_t end, typename F>
        requires (begin >= end) || std::invocable<F, std::integral_constant<size_t, begin>>
    constexpr void static_for(F&& func) noexcept(
        begin >= end || std::is_nothrow_invocable_v<F, std::integral_constant<size_t, begin>>)
    {
        if constexpr (begin < end)
            [&]<size_t... i>(std::index_sequence<i...>)
            {
                (func(std::integral_constant<size_t, i + begin>{}), ...);
            }(std::make_index_sequence<end - begin>{});
    }

    template <size_t begin, size_t end, std::invocable<size_t> F>
        requires (!std::invocable<F, std::integral_constant<size_t, begin>>)
    constexpr void static_for(F&& func) noexcept(std::is_nothrow_invocable_v<F, size_t>)
    {
        if constexpr (begin < end)
            [&]<size_t... i>(std::index_sequence<i...>)
            {
                (func(i + begin), ...);
            }(std::make_index_sequence<end - begin>{});
    }
}
