#include <catch2/catch.hpp>
#include <clu/tag_invoke.h>

namespace mylib
{
    CLU_DEFINE_TAG_INVOKE_CPO(do_things);
}

struct ThingDoer
{
    friend int tag_invoke(mylib::do_things_t, ThingDoer, int) { return 42; }
    friend int tag_invoke(mylib::do_things_t, ThingDoer, int, int) { return 0; }
    friend int tag_invoke(mylib::do_things_t, ThingDoer) noexcept { return 100; }
};

TEST_CASE("customization", "[tag_invoke]")
{
    SECTION("invoke")
    {
        ThingDoer td;
        REQUIRE(mylib::do_things(td, 0) == 42);
        REQUIRE(mylib::do_things(td, 0, 0) == 0);
        REQUIRE(mylib::do_things(td) == 100);
    }
    SECTION("type traits")
    {
        static_assert(clu::nothrow_tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer>);
        static_assert(!clu::nothrow_tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer, int>);
        static_assert(clu::tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer, int>);
        static_assert(!clu::tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer, int, int, int>);
        static_assert(std::is_same_v<clu::tag_invoke_result_t<clu::tag_t<mylib::do_things>, ThingDoer>, int>);
    }
}
