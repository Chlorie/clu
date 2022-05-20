#pragma once

#include <algorithm>
#include <catch2/matchers/catch_matchers_templated.hpp>

namespace clu::testing
{
    template <std::ranges::input_range R>
    struct EqualsRangeMatcher : Catch::Matchers::MatcherGenericBase
    {
        explicit EqualsRangeMatcher(const R& range): range_{range} {}
        bool match(const std::ranges::input_range auto& other) const { return std::ranges::equal(range_, other); }
        std::string describe() const override { return "Equals: " + Catch::rangeToString(range_); }

    private:
        const R& range_;
    };

    template <std::ranges::input_range Range>
    auto EqualsRange(const Range& range)
    {
        return EqualsRangeMatcher<Range>{range};
    }

    template <typename T>
    auto EqualsRange(const std::initializer_list<T> ilist)
    {
        return EqualsRangeMatcher<std::initializer_list<T>>{ilist};
    }
} // namespace clu::testing
