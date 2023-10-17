#include <catch2/catch_test_macros.hpp>

#include "clu/execution/algorithms/basic.h"
#include "clu/execution/algorithms/consumers.h"
#include "clu/execution_contexts.h"

using namespace std::literals;
namespace ex = clu::exec;
namespace tt = clu::this_thread;
namespace meta = clu::meta;

TEST_CASE("start detached", "[execution]")
{
    clu::timer_thread_context thr;
    std::atomic_flag flag;
    ex::start_detached( //
        ex::schedule_after(thr.get_scheduler(), 25ms) //
        | ex::then([&] { REQUIRE(!flag.test_and_set(std::memory_order::relaxed)); }) //
    );
    flag.wait(true, std::memory_order::relaxed);
}

TEST_CASE("execute", "[execution]")
{
    clu::single_thread_context thr;
    std::atomic_flag flag;
    REQUIRE(std::this_thread::get_id() != thr.get_id());
    ex::execute(thr.get_scheduler(),
        [&]
        {
            REQUIRE(std::this_thread::get_id() == thr.get_id());
            flag.test_and_set(std::memory_order::relaxed);
        });
    flag.wait(true, std::memory_order::relaxed);
}

// No need to test sync_wait since it's used everywhere in tests for other algorithms
