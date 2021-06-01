#include "matchers.h"
#include <clu/experimental/generator.h>
#include <array>
#include <ranges>

namespace tst = clu::testing;

clu::generator<int> infinite()
{
    for (int i = 0;; i++)
        co_yield i;
}

using ilist = std::initializer_list<int>;

TEST(Generator, Infinite)
{
    clu::generator<int> iota = infinite();
    std::array<int, 5> results{};
    std::ranges::copy_n(iota.begin(), 5, results.begin());
    EXPECT_THAT(results, tst::elements_are(ilist{ 0, 1, 2, 3, 4 }));
}

clu::generator<int> single(const int value) { co_yield value; }

TEST(Generator, Single)
{
    clu::generator<int> gen = single(0);
    EXPECT_THAT(gen | std::views::all, tst::elements_are(ilist{ 0 }));
}
