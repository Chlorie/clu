#include <catch2/catch_test_macros.hpp>

#include "clu/overload.h"
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
        clu::timer_thread_context timer;
        const auto should_be_cancelled = [&]
        { return ex::schedule_after(timer.get_scheduler(), 50ms) | ex::then([] { FAIL(); }); };

        SECTION("via exception")
        {
            try
            {
                // clang-format off
                tt::sync_wait_with_variant(ex::when_all(
                    ex::schedule_after(timer.get_scheduler(), 25ms)
                    | ex::let_value(
                          [] { return ex::just_error(std::make_exception_ptr(std::runtime_error("yeet"))); }),
                    should_be_cancelled()
                ));
                // clang-format on
                FAIL();
            }
            catch (const std::runtime_error& e)
            {
                REQUIRE(e.what() == "yeet"sv);
            }
        }

        SECTION("via cancellation")
        {
            // clang-format off
            const auto res = tt::sync_wait_with_variant(ex::when_all( //
                ex::schedule_after(timer.get_scheduler(), 25ms) | ex::let_value([] { return ex::stop(); }),
                should_be_cancelled() //
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
        clu::timer_thread_context timer;

        SECTION("same type")
        {
            // clang-format off
            const auto res = tt::sync_wait(ex::when_any( //
                ex::schedule_after(timer.get_scheduler(), 25ms) | ex::then([] { return 1; }),
                ex::schedule_after(timer.get_scheduler(), 50ms) | ex::then([] { return 2; }) //
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
                ex::schedule_after(timer.get_scheduler(), 25ms) | ex::then([] { return "hello"s; }),
                ex::schedule_after(timer.get_scheduler(), 50ms) | ex::then([] { return 42; }) //
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
}
