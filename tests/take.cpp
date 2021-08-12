#include <catch2/catch.hpp>
#include <memory>
#include <string>

#include "clu/take.h"

TEST_CASE("take primitive value", "[take]")
{
    int x = 42;
    REQUIRE(clu::take(x) == 42);
    REQUIRE(x == 0);

    int* ptr = &x;
    REQUIRE(clu::take(ptr) == &x);
    REQUIRE(ptr == nullptr);
}

TEST_CASE("take non-primitive value", "[take]")
{
    auto ptr = std::make_unique<int>();
    int* value = ptr.get();
    REQUIRE(clu::take(ptr).get() == value);
    REQUIRE(ptr == nullptr);

    std::string str = "test";
    REQUIRE(clu::take(str) == "test");
    REQUIRE(str.empty());
}
