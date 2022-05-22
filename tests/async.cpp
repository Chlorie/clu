#include <catch2/catch_test_macros.hpp>
#include <chrono>

#include "clu/async.h"
#include "clu/execution_contexts.h"
#include "clu/task.h"
#include "clu/scope.h"

namespace chr = std::chrono;
namespace ex = clu::exec;
using namespace std::literals;

TEST_CASE("async_manual_reset_event", "[async]")
{
    clu::static_thread_pool tp(3);
    clu::scope_exit g1{[&] { tp.finish(); }};
    clu::timer_thread_context timer;
    clu::scope_exit g2{[&] { timer.finish(); }};
    clu::async::manual_reset_event ev;
    int value = 0;

    const auto producer = [&](const chr::milliseconds delay) -> clu::task<void>
    {
        co_await ex::schedule_after(timer.get_scheduler(), delay);
        value = 42;
        ev.set();
        co_return;
    };

    const auto consumer = [&](const chr::milliseconds delay, int& ref) -> clu::task<void>
    {
        co_await ex::schedule_after(timer.get_scheduler(), delay);
        co_await ev.wait_async();
        ref = value;
    };

    SECTION("producer first")
    {
        value = 0;
        int out1 = 0, out2 = 0;
        ev.reset();
        // clang-format off
        clu::this_thread::sync_wait(
            ex::on(
                tp.get_scheduler(),
                ex::when_all(
                    producer(0ms),
                    consumer(25ms, out1),
                    consumer(25ms, out2)
                )
            )
        );
        // clang-format on
        REQUIRE(out1 == 42);
        REQUIRE(out2 == 42);
    }

    SECTION("consumer first")
    {
        value = 0;
        int out1 = 0, out2 = 0;
        ev.reset();
        // clang-format off
        clu::this_thread::sync_wait(
            ex::on(
                tp.get_scheduler(),
                ex::when_all(
                    producer(25ms),
                    consumer(0ms, out1),
                    consumer(0ms, out2)
                )
            )
        );
        // clang-format on
        REQUIRE(out1 == 42);
        REQUIRE(out2 == 42);
    }
}
