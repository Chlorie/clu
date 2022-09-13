#include <catch2/catch_test_macros.hpp>

#include "clu/ordered_hash_set.h"
#include "matchers.h"

namespace ct = clu::testing;

TEST_CASE("push_back", "[ordered_hash_set]")
{
    clu::ordered_hash_multiset<int> set;
    set.push_back(4);
    set.push_back(5);
    set.push_back(3);
    set.push_back(6);
    set.push_back(2);
    set.push_back(5);
    set.push_back(1);
    REQUIRE_THAT(set, ct::EqualsRange({4, 5, 3, 6, 2, 5, 1}));
}
