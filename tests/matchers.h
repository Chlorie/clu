#pragma once

#ifdef _HAS_DEPRECATED_RESULT_OF
#undef _HAS_DEPRECATED_RESULT_OF
#endif
#define _HAS_DEPRECATED_RESULT_OF 1 // Workaround
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include <gmock/gmock.h>
#undef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#undef _HAS_DEPRECATED_RESULT_OF

#include <algorithm>

namespace clu::testing
{
    namespace gtst = ::testing;

    MATCHER_P(elements_are, elements, "")
    {
        if (std::ranges::equal(arg, elements)) return true;
        if constexpr (std::ranges::forward_range<decltype(arg)>)
        {
            *result_listener << "contents:";
            for (auto&& i : arg) *result_listener << ' ' << gtst::PrintToString(i);
        }
        return false;
    }
}
