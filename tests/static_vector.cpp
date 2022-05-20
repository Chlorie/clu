#include <catch2/catch_test_macros.hpp>

#include "clu/static_vector.h"
#include "lifetime_counter.h"
#include "matchers.h"

namespace ct = clu::testing;

TEST_CASE("static vector constructors", "[static_vector]")
{
    SECTION("default")
    {
        constexpr size_t capacity = 4;
        const clu::static_vector<int, capacity> vi;
        REQUIRE(vi.empty());
        REQUIRE(vi.size() == 0); // NOLINT(readability-container-size-empty)
        REQUIRE(vi.capacity() == capacity);
        REQUIRE(vi.max_size() == capacity);

        ct::LifetimeCounter counter;
        {
            ct::LifetimeCountingScope scope(counter);
            const clu::static_vector<ct::LifetimeNotifier, capacity> vec;
        }
        REQUIRE(counter.total_ctor() == 0);
        REQUIRE(counter.alive() == 0);
    }

    SECTION("fill")
    {
        constexpr size_t capacity = 4;
        constexpr size_t size = 3;
        using vecint = clu::static_vector<int, capacity>;
        using vecltc = clu::static_vector<ct::LifetimeNotifier, capacity>;
        {
            const vecint vec1(size);
            REQUIRE(vec1.size() == size);
            REQUIRE_THAT(vec1, ct::EqualsRange({0, 0, 0}));
            const vecint vec2(size, 42);
            REQUIRE(vec2.size() == size);
            REQUIRE_THAT(vec2, ct::EqualsRange({42, 42, 42}));
        }
        {
            ct::LifetimeCounter counter;
            {
                ct::LifetimeCountingScope scope(counter);
                const vecltc vec(size);
                REQUIRE(counter.def_ctor == size);
            }
            REQUIRE(counter.alive() == 0);
        }
        {
            ct::LifetimeCounter counter;
            {
                ct::LifetimeCountingScope scope(counter);
                const vecltc vec(size, ct::LifetimeNotifier());
                REQUIRE(counter.copy_ctor == size);
            }
            REQUIRE(counter.alive() == 0);
        }
    }

    SECTION("iterator pair")
    {
        using vector_t = clu::static_vector<int, 4>;
        const std::vector arr{1, 2, 3};
        const vector_t common(arr.begin(), arr.end());
        REQUIRE_THAT(common, ct::EqualsRange(arr));
        auto view = std::views::filter(arr, [](const int i) { return i % 2 != 0; });
        const vector_t uncommon(view.begin(), view.end());
        REQUIRE_THAT(uncommon, ct::EqualsRange({1, 3}));
    }

    SECTION("range")
    {
        using vector_t = clu::static_vector<int, 4>;
        const std::vector arr{1, 2, 3};
        const vector_t common(arr);
        REQUIRE_THAT(common, ct::EqualsRange(arr));
        auto view = std::views::filter(arr, [](const int i) { return i % 2 != 0; });
        const vector_t uncommon(view);
        REQUIRE_THAT(uncommon, ct::EqualsRange({1, 3}));
    }
}

// TODO: more tests
