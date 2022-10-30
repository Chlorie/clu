#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "clu/event.h"

TEST_CASE("event", "[event]")
{
    int v = 0;
    clu::event<int> ev;

    ev += [&v](const int i) { v += i; };
    struct big_functor
    {
        int& v;
        char dummy[24]{};
        void operator()(const int i) { v += 2 * i; }
    };
    const auto tag = (ev += big_functor{v});
    ev(42);
    REQUIRE(v == 126);

    v = 0;
    ev.unsubscribe(tag);
    ev(42);
    REQUIRE(v == 42);
}
