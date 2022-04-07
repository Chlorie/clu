#include <catch2/catch.hpp>
#include "clu/execution/algorithms.h"

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
            const auto res = tt::sync_wait( //
                ex::just(21) //
                | ex::then([](const int v) { return v * 2; }) //
            );
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
            const auto res = tt::sync_wait(ex::stop() | adaptor);
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

TEST_CASE("transfer", "[execution]")
{
    SECTION("switch thread")
    {
        ex::single_thread_context ctx;
        std::thread::id id1, id2;
        tt::sync_wait( //
            ex::just_from([&] { id1 = std::this_thread::get_id(); }) //
            | ex::transfer(ctx.get_scheduler()) //
            | ex::then([&] { id2 = std::this_thread::get_id(); }) //
        );
        REQUIRE(id1 == std::this_thread::get_id());
        REQUIRE(id2 == ctx.get_id());
    }
}

TEST_CASE("when all", "[execution]")
{
    SECTION("zero")
    {
        const auto res = tt::sync_wait(ex::when_all());
        REQUIRE(res);
        STATIC_REQUIRE(std::tuple_size_v<std::decay_t<decltype(*res)>> == 0);
    }

    SECTION("just concatenation")
    {
        const auto res = tt::sync_wait(ex::when_all( //
            ex::just(1), //
            ex::just(), //
            ex::just(42., nullptr) //
            ));
        REQUIRE(res);
        STATIC_REQUIRE(std::tuple_size_v<std::decay_t<decltype(*res)>> == 3);
        const auto [i, d, n] = *res;
        REQUIRE((i == 1 && d == 42. && n == nullptr));
    }

    SECTION("no value")
    {
        const auto res = tt::sync_wait_with_variant(ex::when_all( //
            ex::just(1), //
            ex::just_stopped() //
            ));
        REQUIRE_FALSE(res); // is stopped
    }

    SECTION("basic usage") {}
}

// TODO: more tests!
