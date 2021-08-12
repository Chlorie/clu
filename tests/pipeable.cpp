#include <catch2/catch.hpp>
#include <clu/macros.h>
#include <clu/pipeable.h>

constexpr auto add = clu::make_pipeable([](const int a, const int b) noexcept { return a + b; });

TEST_CASE("non-overloaded pipes", "[pipeable]")
{
    REQUIRE(add(40, 2) == 42);
    REQUIRE((40 | add(2)) == 42);
    REQUIRE(add(2)(40) == 42);
    REQUIRE((1 | add(2) | add(3)) == 6);
}

template <typename T, typename U>
concept add_pipeable = requires(T t, U u) { t | add(u); };

TEST_CASE("pipeable SFINAE friendliness", "[pipeable]")
{
    struct S {};
    static_assert(std::is_nothrow_invocable_v<decltype(add), int, int>);
    static_assert(add_pipeable<int, int>);
    static_assert(!add_pipeable<int, S>);
    static_assert(!add_pipeable<S, int>);
}

// MSVC fails to compile this for some weird reason
#ifndef CLU_MSVC_COMPILERS
TEST_CASE("compound pipes", "[pipeable]")
{
    const auto add42 = add(40) | add(2);
    REQUIRE(add42(0) == 42);
    REQUIRE((40 | add42) == 82);
}
#endif
