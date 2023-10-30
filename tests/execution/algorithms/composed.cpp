#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "clu/execution/algorithms.h"
#include "clu/execution_contexts.h"
#include "clu/task.h"

namespace ex = clu::exec;
namespace tt = clu::this_thread;
namespace meta = clu::meta;
using namespace std::literals;

TEST_CASE("transfer", "[execution]")
{
    // This is just the test for schedule_from
    SECTION("switch thread")
    {
        clu::single_thread_context ctx;
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

TEST_CASE("transfer just", "[execution]")
{
    SECTION("switch thread")
    {
        clu::single_thread_context ctx;
        std::thread::id id;
        const auto res = tt::sync_wait( //
            ex::transfer_just(ctx.get_scheduler(), 42) //
            | ex::then(
                  [&](const int value)
                  {
                      id = std::this_thread::get_id();
                      return value;
                  }) //
        );
        REQUIRE(res);
        REQUIRE(id == ctx.get_id());
        REQUIRE(id != std::this_thread::get_id());
        const auto [i] = *res;
        REQUIRE(i == 42);
    }
}

TEST_CASE("just from", "[execution]")
{
    const auto res = tt::sync_wait(ex::just_from([] { return 42; }));
    REQUIRE(res);
    const auto [i] = *res;
    REQUIRE(i == 42);
}

TEST_CASE("defer", "[execution]")
{
    SECTION("correct result")
    {
        const auto res = tt::sync_wait(ex::defer([] { return ex::just(42); }));
        REQUIRE(res);
        const auto [i] = *res;
        REQUIRE(i == 42);
    }

    SECTION("deferred sender reconnection")
    {
        int i = 0;
        auto task = [&]() mutable -> clu::task<int> { co_return i++; };
        const auto deferred = ex::defer(task);
        const auto res1 = tt::sync_wait(deferred);
        REQUIRE(res1);
        const auto [i1] = *res1;
        REQUIRE(i1 == 0);
        const auto res2 = tt::sync_wait(deferred);
        REQUIRE(res2);
        const auto [i2] = *res2;
        REQUIRE(i2 == 1);
    }
}

TEST_CASE("stopped as", "[execution]")
{
    const auto task = [](const bool stop) -> clu::task<int>
    {
        if (stop)
            co_await ex::stop();
        co_return 42;
    };

    SECTION("optional")
    {
        SECTION("not stopped")
        {
            tt::sync_wait( //
                task(false) //
                | ex::stopped_as_optional() //
                | ex::then([](const std::optional<int> opt) { REQUIRE(opt == 42); }) //
            );
        }

        SECTION("stopped")
        {
            tt::sync_wait( //
                task(true) //
                | ex::stopped_as_optional() //
                | ex::then([](const std::optional<int> opt) { REQUIRE_FALSE(opt); }) //
            );
        }
    }

    SECTION("error")
    {
        SECTION("not stopped")
        {
            tt::sync_wait( //
                task(false) //
                | ex::stopped_as_error("oh no"sv) //
                | ex::then([](const int i) { REQUIRE(i == 42); }) //
                | ex::upon_error([](auto&&) { FAIL(); }) //
            );
        }

        SECTION("stopped")
        {
            // clang-format off
            REQUIRE_THROWS_WITH(tt::sync_wait(
                task(true)
                | ex::stopped_as_error(std::make_exception_ptr(std::runtime_error("oh no!")))
                | ex::then([](int) { FAIL(); })
            ), "oh no!");
            // clang-format on
        }
    }
}

TEST_CASE("with stop token", "[execution]")
{
    // Just the same test case as with_query_value
    const auto res = tt::sync_wait( //
        ex::with_stop_token(ex::read(clu::get_stop_token), clu::in_place_stop_token{}) //
    );
    REQUIRE(res);
    const auto [token] = *res;
    STATIC_REQUIRE(std::same_as<std::decay_t<decltype(token)>, clu::in_place_stop_token>);
    REQUIRE(!token.stop_possible());
}

TEST_CASE("finally", "[execution]")
{
    enum result_type
    {
        value,
        error,
        stopped
    };
    const auto task = [](const result_type type) -> clu::task<int>
    {
        if (type == error)
            throw std::runtime_error("yeet");
        if (type == stopped)
            co_await ex::stop();
        co_return 42;
    };
    bool finally_called = false;
    const auto cleanup = ex::just_from([&] { finally_called = true; });

    SECTION("source completes")
    {
        const auto res = tt::sync_wait(task(value) | ex::finally(cleanup));
        REQUIRE(res);
        REQUIRE(finally_called);
        const auto [i] = *res;
        REQUIRE(i == 42);
    }

    SECTION("source errs")
    {
        REQUIRE_THROWS_WITH(tt::sync_wait(task(error) | ex::finally(cleanup)), "yeet");
        REQUIRE(finally_called);
    }

    SECTION("source stops")
    {
        const auto res = tt::sync_wait(task(stopped) | ex::finally(cleanup));
        REQUIRE_FALSE(res);
        REQUIRE(finally_called);
    }

    SECTION("cleanup errs")
    {
        REQUIRE_THROWS_WITH( //
            tt::sync_wait(task(value) //
                | ex::finally(cleanup | ex::then([] { throw std::runtime_error("oh no!"); }))), //
            "oh no!");
        REQUIRE(finally_called);
    }

    SECTION("cleanup stops")
    {
        const auto res = tt::sync_wait_with_variant(task(value) //
            | ex::finally(cleanup | ex::let_value([] { return ex::stop(); })));
        REQUIRE_FALSE(res);
        REQUIRE(finally_called);
    }
}

TEST_CASE("into tuple", "[execution]")
{
    const auto task = [](const bool err)
    {
        return ex::just_from(
                   [=]
                   {
                       if (err)
                           throw std::runtime_error("oh no!");
                   }) //
            | ex::let_value([] { return ex::just(42, "wow"sv); }) //
            | ex::into_tuple();
    };

    SECTION("value")
    {
        const auto res = tt::sync_wait(task(false));
        REQUIRE(res);
        const auto& [tup] = *res;
        const auto& [i, s] = tup;
        REQUIRE(i == 42);
        REQUIRE(s == "wow");
    }

    SECTION("error") { REQUIRE_THROWS_WITH(tt::sync_wait(task(true)), "oh no!"); }

    SECTION("stopped")
    {
        const auto res = tt::sync_wait_with_variant(ex::stop() | ex::into_tuple());
        REQUIRE_FALSE(res);
    }
}
