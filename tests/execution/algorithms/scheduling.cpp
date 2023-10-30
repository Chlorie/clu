#include <catch2/catch_test_macros.hpp>

#include "clu/execution/algorithms.h"
#include "clu/execution_contexts.h"

namespace ex = clu::exec;
namespace tt = clu::this_thread;
namespace meta = clu::meta;

TEST_CASE("on", "[execution]")
{
    clu::single_thread_context ctx;
    const auto ctx_id = ctx.get_id();

    SECTION("on correct thread")
    {
        std::thread::id id;
        tt::sync_wait(ex::on(ctx.get_scheduler(), //
            ex::just_from([&] { id = std::this_thread::get_id(); })));
        REQUIRE(id == ctx_id);
    }

    SECTION("receiver env query")
    {
        tt::sync_wait(ex::on( //
            ctx.get_scheduler(),
            ex::get_scheduler() //
                | ex::then([&](const ex::scheduler auto schd) //
                      { REQUIRE(schd == ctx.get_scheduler()); }) //
            ));
    }
}

TEST_CASE("schedule from", "[execution]")
{
    SECTION("switch thread")
    {
        clu::single_thread_context ctx;
        std::thread::id id1, id2;
        tt::sync_wait( //
            ex::schedule_from(ctx.get_scheduler(), ex::just_from([&] { id1 = std::this_thread::get_id(); })) //
            | ex::then([&] { id2 = std::this_thread::get_id(); }) //
        );
        REQUIRE(id1 == std::this_thread::get_id());
        REQUIRE(id2 == ctx.get_id());
    }
}
