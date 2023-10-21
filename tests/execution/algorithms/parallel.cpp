#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "clu/execution/algorithms/basic.h"
#include "clu/execution/algorithms/consumers.h"
#include "clu/execution/algorithms/composed.h"
#include "clu/execution_contexts.h"
#include "clu/task.h"

using namespace std::literals;
using enum std::memory_order;
namespace ex = clu::exec;
namespace tt = clu::this_thread;
namespace meta = clu::meta;

clu::timer_thread_context timer;
const auto wait_short = [] { return ex::schedule_after(timer.get_scheduler(), 25ms); };
const auto wait_long = [] { return ex::schedule_after(timer.get_scheduler(), 50ms); };
const auto then_fail = [] { return ex::then([] { FAIL(); }); };
const auto then_throw = [](const char* msg) { return ex::then([=] { throw std::runtime_error(msg); }); };
const auto then_stop = [] { return ex::let_value([] { return ex::stop(); }); };

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

    SECTION("all completed")
    {
        std::atomic_size_t count = 0;
        const auto res = tt::sync_wait(ex::when_all( //
            ex::just_from([&] { count.fetch_add(1, relaxed); }), //
            ex::just_from([&] { count.fetch_add(2, relaxed); }), //
            ex::just_from([&] { count.fetch_add(3, relaxed); }) //
            ));
        REQUIRE(res);
        REQUIRE(count.load(relaxed) == 6);
    }

    SECTION("early exit")
    {
        SECTION("via exception")
        {
            // clang-format off
            REQUIRE_THROWS_WITH( //
                tt::sync_wait(ex::when_all(
                    wait_short() | then_throw("yeet"),
                    wait_long() | then_fail()
                )),
                "yeet"
            );
            // clang-format on
        }

        SECTION("via cancellation")
        {
            // clang-format off
            const auto res = tt::sync_wait_with_variant(ex::when_all( //
                wait_short() | then_stop(), //
                wait_long() | then_fail()
            ));
            // clang-format on
            REQUIRE_FALSE(res);
        }
    }
}

TEST_CASE("when any", "[execution]")
{
    SECTION("zero")
    {
        const auto res = tt::sync_wait(ex::when_any());
        REQUIRE(res);
        STATIC_REQUIRE(std::tuple_size_v<std::decay_t<decltype(*res)>> == 0);
    }

    SECTION("all values")
    {
        SECTION("same type")
        {
            // clang-format off
            const auto res = tt::sync_wait(ex::when_any( //
                wait_short() | ex::then([] { return 1; }),
                wait_long() | ex::then([] { return 2; }) //
            ));
            // clang-format on
            REQUIRE(res);
            const auto [i] = *res;
            REQUIRE(i == 1);
        }

        SECTION("different types")
        {
            // clang-format off
            const auto res = tt::sync_wait_with_variant(ex::when_any( //
                wait_short() | ex::then([] { return "hello"s; }),
                wait_long() | ex::then([] { return 42; }) //
            ));
            // clang-format on
            REQUIRE(res);
            std::visit(
                []<typename T>(const std::tuple<T>& tuple)
                {
                    if constexpr (std::is_same_v<T, int>)
                        FAIL();
                    else
                        REQUIRE(std::get<0>(tuple) == "hello");
                },
                *res);
        }
    }

    SECTION("fail to complete")
    {
        SECTION("all stopped")
        {
            const auto res = tt::sync_wait_with_variant(ex::when_any(ex::stop(), ex::stop()));
            REQUIRE_FALSE(res);
        }

        SECTION("all errors")
        {
            // The first one counts, the rest gets ignored
            REQUIRE_THROWS_WITH( //
                tt::sync_wait(ex::when_any( //
                    wait_short() | then_throw("short"), //
                    wait_long() | then_throw("long") //
                    )),
                "short");
        }

        SECTION("error first")
        {
            REQUIRE_THROWS_WITH( //
                tt::sync_wait(ex::when_any( //
                    wait_short() | then_throw("oh no!"), //
                    wait_long() | then_stop() //
                    )),
                "oh no!");
        }

        SECTION("stop first")
        {
            REQUIRE_THROWS_WITH( //
                tt::sync_wait(ex::when_any( //
                    wait_long() | then_throw("oh no!"), //
                    wait_short() | then_stop() //
                    )),
                "oh no!");
        }
    }

    SECTION("non-value before first value")
    {
        SECTION("stop first")
        {
            const auto res = tt::sync_wait(ex::when_any( //
                wait_long() | ex::then([] { return 42; }), //
                wait_short() | then_stop() //
                ));
            REQUIRE(res);
            const auto [i] = *res;
            REQUIRE(i == 42);
        }

        SECTION("stop first")
        {
            const auto res = tt::sync_wait(ex::when_any( //
                wait_long() | ex::then([] { return 42; }), //
                wait_short() | ex::let_value([] { return ex::just_error("yeet!"sv); }) //
                ));
            REQUIRE(res);
            const auto [i] = *res;
            REQUIRE(i == 42);
        }
    }
}

TEST_CASE("stop when", "[execution]")
{
    SECTION("source finishes first")
    {
        SECTION("with value")
        {
            const auto res = tt::sync_wait(ex::stop_when( //
                wait_short() | ex::then([] { return 42; }), //
                wait_long() | then_fail() //
                ));
            REQUIRE(res);
            const auto [i] = *res;
            REQUIRE(i == 42);
        }

        SECTION("with error")
        {
            REQUIRE_THROWS_WITH( //
                tt::sync_wait(ex::stop_when( //
                    wait_short() | then_throw("oh no!"), //
                    wait_long() | then_fail() //
                    )),
                "oh no!");
        }

        SECTION("with stopped")
        {
            const auto res = tt::sync_wait_with_variant(ex::stop_when( //
                wait_short() | then_stop(), //
                wait_long() | then_fail() //
                ));
            REQUIRE_FALSE(res);
        }
    }

    SECTION("trigger finishes first")
    {
        SECTION("with value")
        {
            const auto res = tt::sync_wait(ex::stop_when( //
                wait_long() | then_fail(), //
                wait_short() //
                ));
            REQUIRE_FALSE(res);
        }

        SECTION("with error")
        {
            REQUIRE_THROWS_WITH( //
                tt::sync_wait(ex::stop_when( //
                    wait_long() | then_fail(), //
                    wait_short() | then_throw("oh no!") //
                    )),
                "oh no!");
        }

        SECTION("with stopped")
        {
            const auto res = tt::sync_wait_with_variant(ex::stop_when( //
                wait_long() | then_fail(), //
                wait_short() | then_stop() //
                ));
            REQUIRE_FALSE(res);
        }
    }
}
