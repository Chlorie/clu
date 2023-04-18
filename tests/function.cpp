#include <catch2/catch_test_macros.hpp>

#include "clu/function.h"
#include "tracking_memory_resource.h"

namespace ct = clu::testing;

TEST_CASE("empty function", "[function]")
{
    const clu::move_only_function<void()> f;
    CHECK_FALSE(f);
    const clu::function<void()> g;
    CHECK_FALSE(g);
}

TEST_CASE("function copyability", "[function]")
{
    STATIC_REQUIRE(std::is_move_constructible_v<clu::move_only_function<void()>>);
    STATIC_REQUIRE(std::is_move_constructible_v<clu::function<void()>>);
    STATIC_REQUIRE(!std::is_copy_constructible_v<clu::move_only_function<void()>>);
    STATIC_REQUIRE(std::is_copy_constructible_v<clu::function<void()>>);
}

int func(const int v) noexcept { return v; }

template <std::size_t N>
struct Func
{
    char dummy[N]{};
    int operator()(const int v) const noexcept { return v; }
};

TEST_CASE("conversion from invocable", "[function]")
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    using F = clu::move_only_function<int(int), std::pmr::polymorphic_allocator<>>;

    CHECK(mem.bytes_allocated() == 0);

    SECTION("C function")
    {
        F f(func, alloc);
        CHECK(f);
        CHECK(f.get_allocator() == alloc);
        CHECK(f(42) == 42);
        CHECK(mem.bytes_allocated() == 0);
    }

    SECTION("Small function object")
    {
        F f(Func<1>{}, alloc);
        CHECK(f);
        CHECK(f.get_allocator() == alloc);
        CHECK(f(42) == 42);
        CHECK(mem.bytes_allocated() == 0);
    }

    SECTION("Small function object")
    {
        F f(Func<64>{}, alloc);
        CHECK(f);
        CHECK(f.get_allocator() == alloc);
        CHECK(f(42) == 42);
        CHECK(mem.bytes_allocated() == 64);
    }

    CHECK(mem.bytes_allocated() == 0);
}
