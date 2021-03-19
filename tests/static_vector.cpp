#define _HAS_DEPRECATED_RESULT_OF 1 // Workaround
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#include <gmock/gmock.h>
#undef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#undef _HAS_DEPRECATED_RESULT_OF

#include <gtest/gtest.h>
#include <array>
#include <ranges>

#include "clu/static_vector.h"
#include "lifetime_counter.h"

namespace tst = testing;
namespace clt = clu::testing;

TEST(StaticVectorCtor, Default)
{
    constexpr size_t capacity = 4;
    const clu::static_vector<int, capacity> vi;
    EXPECT_TRUE(vi.empty());
    EXPECT_THAT(vi, tst::SizeIs(0));
    EXPECT_EQ(vi.capacity(), capacity);
    EXPECT_EQ(vi.max_size(), capacity);

    clt::LifetimeCounter counter;
    {
        clt::LifetimeCountingScope scope(counter);
        const clu::static_vector<clt::LifetimeNotifier, capacity> vec;
    }
    EXPECT_EQ(counter.total_ctor(), 0);
    EXPECT_EQ(counter.alive(), 0);
}

TEST(StaticVectorCtor, Fill)
{
    constexpr size_t capacity = 4;
    constexpr size_t size = 3;
    using vecint = clu::static_vector<int, capacity>;
    using vecltc = clu::static_vector<clt::LifetimeNotifier, capacity>;
    {
        const vecint vec1(size);
        EXPECT_THAT(vec1, tst::SizeIs(size));
        EXPECT_THAT(vec1, tst::ElementsAre(0, 0, 0));
        const vecint vec2(size, 42);
        EXPECT_THAT(vec2, tst::SizeIs(size));
        EXPECT_THAT(vec2, tst::ElementsAre(42, 42, 42));
    }
    {
        clt::LifetimeCounter counter;
        {
            clt::LifetimeCountingScope scope(counter);
            const vecltc vec(size);
            EXPECT_EQ(counter.def_ctor, size);
        }
        EXPECT_EQ(counter.alive(), 0);
    }
    {
        clt::LifetimeCounter counter;
        {
            clt::LifetimeCountingScope scope(counter);
            const vecltc vec(size, clt::LifetimeNotifier());
            EXPECT_EQ(counter.copy_ctor, size);
        }
        EXPECT_EQ(counter.alive(), 0);
    }
}

TEST(StaticVectorCtor, IterPair)
{
    using vector_t = clu::static_vector<int, 4>;
    const std::array arr{ 1, 2, 3 };
    const vector_t common(arr.begin(), arr.end());
    EXPECT_THAT(common, tst::ElementsAreArray(arr));
    auto view = std::views::filter(arr, [](const int i) { return i % 2 != 0; });
    const vector_t uncommon(view.begin(), view.end());
    EXPECT_THAT(uncommon, tst::ElementsAre(1, 3));
}

TEST(StaticVectorCtor, Range)
{
    using vector_t = clu::static_vector<int, 4>;
    const std::array arr{ 1, 2, 3 };
    const vector_t common(arr);
    EXPECT_THAT(common, tst::ElementsAreArray(arr));
    auto view = std::views::filter(arr, [](const int i) { return i % 2 != 0; });
    const vector_t uncommon(view);
    EXPECT_THAT(uncommon, tst::ElementsAre(1, 3));
}
