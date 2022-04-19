#include <catch2/catch.hpp>

#include "clu/tag_invoke.h"

namespace mylib
{
    inline struct do_things_t
    {
        template <typename... Args>
            requires clu::tag_invocable<do_things_t, Args&&...>
        constexpr auto operator()(Args&&... args) const noexcept( //
            clu::nothrow_tag_invocable<do_things_t, Args&&...>) //
            -> clu::tag_invoke_result_t<do_things_t, Args&&...>
        {
            return clu::tag_invoke(*this, static_cast<Args&&>(args)...);
        }
    } constexpr do_things{};
} // namespace mylib

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
        STATIC_REQUIRE(clu::nothrow_tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer>);
        STATIC_REQUIRE(!clu::nothrow_tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer, int>);
        STATIC_REQUIRE(clu::tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer, int>);
        STATIC_REQUIRE(!clu::tag_invocable<clu::tag_t<mylib::do_things>, ThingDoer, int, int, int>);
        STATIC_REQUIRE(std::is_same_v<clu::tag_invoke_result_t<clu::tag_t<mylib::do_things>, ThingDoer>, int>);
    }
}
