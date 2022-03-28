#include <catch2/catch.hpp>
#include <clu/execution/algorithms.h>

namespace ex = clu::exec;
namespace tt = clu::this_thread;
namespace meta = clu::meta;

TEST_CASE("just", "[execution]")
{
    SECTION("just")
    {
        SECTION("one value")
        {
            const auto res = tt::sync_wait(ex::just(42));
            REQUIRE(res); // is not stopped
            const auto [v] = *res;
            REQUIRE(v == 42); // the result is correct
        }
        SECTION("no value")
        {
            const auto res = tt::sync_wait(ex::just());
            REQUIRE(res);
        }
        SECTION("multiple values")
        {
            const auto res = tt::sync_wait(ex::just(1, 2.));
            REQUIRE(res);
            const auto [i, d] = *res;
            REQUIRE(i == 1);
            REQUIRE(d == 2.);
        }
    }
    SECTION("just error")
    {
        try
        {
            tt::sync_wait_with_variant(ex::just_error(42));
            FAIL(); // should not reach here
        }
        catch (const int e)
        {
            REQUIRE(e == 42);
        }
    }
    SECTION("just stopped")
    {
        const auto res = tt::sync_wait_with_variant(ex::just_stopped());
        REQUIRE_FALSE(res); // is stopped
    }
}

TEST_CASE("upon", "[execution]")
{
    SECTION("then")
    {
        SECTION("check result")
        {
            // clang-format off
            const auto res = tt::sync_wait(
                ex::just(21)
                | ex::then([](const int v) { return v * 2; })
            );
            // clang-format on
            REQUIRE(res);
            const auto [v] = *res;
            REQUIRE(v == 42);
        }
        SECTION("noexcept-ness")
        {
            using nothrow_sender = decltype(ex::just() | ex::then([]() noexcept {}));
            STATIC_REQUIRE(meta::set_equal_lv<ex::completion_signatures_of_t<nothrow_sender>,
                ex::completion_signatures<ex::set_value_t()>>);
            using may_throw_sender = decltype(ex::just() | ex::then([] {}));
            STATIC_REQUIRE(meta::set_equal_lv<ex::completion_signatures_of_t<may_throw_sender>,
                ex::completion_signatures<ex::set_value_t(), ex::set_error_t(std::exception_ptr)>>);
        }
    }
    SECTION("upon error")
    {
        const auto adaptor = ex::upon_error([](const int e) { return e * 2; });
        SECTION("pass through")
        {
            const auto res = tt::sync_wait(ex::just(42) | adaptor);
            REQUIRE(res);
            const auto [v] = *res;
            REQUIRE(v == 42);
        }
        SECTION("transform")
        {
            const auto res = tt::sync_wait(ex::just_error(21) | adaptor);
            REQUIRE(res);
            const auto [v] = *res;
            REQUIRE(v == 42);
        }
    }
    SECTION("upon stopped")
    {
        const auto adaptor = ex::upon_stopped([] { return 42; });
        SECTION("pass through")
        {
            const auto res = tt::sync_wait(ex::just(10) | adaptor);
            REQUIRE(res);
            const auto [v] = *res;
            REQUIRE(v == 10);
        }
        SECTION("transform")
        {
            const auto res = tt::sync_wait(ex::stop | adaptor);
            REQUIRE(res);
            const auto [v] = *res;
            REQUIRE(v == 42);
        }
    }
}

TEST_CASE("let", "[execution]")
{
    SECTION("let value") {}
}

// TODO: more tests!
