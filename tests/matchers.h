#pragma once

#include <vector>
#include <ranges>
#include <algorithm>

namespace clu::testing
{
    template <std::ranges::input_range Range>
    auto to_vector(Range&& range)
    {
        std::vector<std::ranges::range_value_t<Range>> result;
        if constexpr (std::ranges::sized_range<Range>)
            result.reserve(std::ranges::size(range));
        {
            auto&& begin = range.begin();
            auto&& end = range.end();
            while (true)
            {
                const bool is_end = begin == end;
                if (is_end) break;
                result.emplace_back(*begin);
                ++begin;
            }
        }
        // std::ranges::copy(static_cast<Range&&>(range), std::back_inserter(result));
        return result;
    }
}
