#pragma once

#include <utility>

namespace clu
{
    template <size_t begin, size_t end, std::invocable<size_t> F>
    constexpr void unrolled_for(F&& func) noexcept(std::is_nothrow_invocable_v<F, size_t>)
    {
        [&]<size_t... i>(std::index_sequence<i...>)
        {
            (func(i + begin), ...);
        }(std::make_index_sequence<end - begin>{});
    }
}
