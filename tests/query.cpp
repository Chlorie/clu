#include <catch2/catch_test_macros.hpp>

#include "clu/query.h"

using namespace std::literals;

struct query1_t
{
    CLU_FORWARDING_QUERY(query1_t);

    template <typename T>
    CLU_STATIC_CALL_OPERATOR(auto)
    (const T& value)
    {
        if constexpr (clu::tag_invocable<query1_t, const T&>)
            return clu::tag_invoke(query1_t{}, value);
        else
            return 0;
    }
} inline constexpr query1;

struct query2_t
{
    template <typename T>
        requires clu::tag_invocable<query2_t, const T&>
    CLU_STATIC_CALL_OPERATOR(auto)(const T& value)
    {
        return clu::tag_invoke(query2_t{}, value);
    }
} inline constexpr query2;

struct env
{
    int value = 0;
    int tag_invoke(query1_t) const { return value; }
    std::string tag_invoke(query2_t) const { return "hello"; }
};

TEST_CASE("querying environments", "[query]")
{
    SECTION("empty")
    {
        REQUIRE(query1(clu::empty_env{}) == 0);
        STATIC_REQUIRE_FALSE(clu::callable<query2_t, clu::empty_env>);
    }
    SECTION("custom environment")
    {
        constexpr env e{.value = 42};
        REQUIRE(query1(e) == 42);
        REQUIRE(query2(e) == "hello");
    }
}

TEST_CASE("query adaptation", "[query]")
{
    SECTION("basic")
    {
        const auto e1 = clu::adapt_env(clu::empty_env{}, clu::query_value{query1, 42});
        REQUIRE(query1(e1) == 42);
        const auto e2 = clu::adapt_env(clu::empty_env{}, clu::query_value{query2, "hi"s});
        REQUIRE(query2(e2) == "hi");
    }
    SECTION("adaptation drops non-forwarding queries")
    {
        const auto e = clu::adapt_env(env{.value = 42});
        REQUIRE(query1(e) == 42);
        STATIC_REQUIRE_FALSE(clu::callable<query2_t, decltype(e)>);
    }
    SECTION("adaptation overwrites")
    {
        SECTION("existing query")
        {
            const auto e = clu::adapt_env(env{.value = 42}, clu::query_value{query1, 43});
            REQUIRE(query1(e) == 43);
        }
        SECTION("switch type with existing query")
        {
            const auto e = clu::adapt_env(env{.value = 42}, clu::query_value{query1, "what"s});
            REQUIRE(query1(e) == "what");
        }
        SECTION("re-adaptation")
        {
            const auto e1 = clu::adapt_env(env{.value = 42}, clu::query_value{query1, 43});
            const auto e2 = clu::adapt_env(e1, clu::query_value{query1, 44});
            REQUIRE(query1(e2) == 44);
        }
        SECTION("re-adaptation and switch type")
        {
            const auto e1 = clu::adapt_env(env{.value = 42}, clu::query_value{query1, 43});
            const auto e2 = clu::adapt_env(e1, clu::query_value{query1, "what"s});
            REQUIRE(query1(e2) == "what");
        }
    }
}
