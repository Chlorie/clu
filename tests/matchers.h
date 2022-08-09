#pragma once

#include <algorithm>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include "clu/concepts.h"

namespace clu::testing
{
    template <std::ranges::input_range R>
    class EqualsRangeMatcher final : public Catch::Matchers::MatcherGenericBase
    {
    public:
        template <similar_to<R> R2>
        explicit EqualsRangeMatcher(R2&& range): range_(static_cast<R2&&>(range))
        {
        }

        template <std::ranges::input_range R2>
        bool match(R2&& other) const
        {
            return std::ranges::equal(range_, static_cast<R2&&>(other));
        }

        std::string describe() const override { return "Equals: " + Catch::rangeToString(range_); }

    private:
        R range_;
    };

    template <std::ranges::input_range Range>
    auto EqualsRange(Range&& range)
    {
        return EqualsRangeMatcher<Range>(static_cast<Range&&>(range));
    }

    template <typename T>
    auto EqualsRange(const std::initializer_list<T> ilist)
    {
        return EqualsRangeMatcher<std::initializer_list<T>>{ilist};
    }
} // namespace clu::testing
