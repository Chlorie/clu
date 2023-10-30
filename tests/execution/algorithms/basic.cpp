#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "clu/overload.h"
#include "clu/execution/algorithms.h"
#include "clu/execution_contexts.h"
#include "clu/task.h"

using namespace std::literals;
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

TEST_CASE("stop if requested", "[execution]")
{
    SECTION("no token")
    {
        const auto res = tt::sync_wait(ex::stop_if_requested());
        REQUIRE(res);
    }
    SECTION("not stopped")
    {
        clu::in_place_stop_source src;
        const auto res = tt::sync_wait(ex::with_stop_token(ex::stop_if_requested(), src.get_token()));
        REQUIRE(res);
    }
    SECTION("stopped")
    {
        clu::in_place_stop_source src;
        src.request_stop();
        const auto res = tt::sync_wait(ex::with_stop_token(ex::stop_if_requested(), src.get_token()));
        REQUIRE_FALSE(res);
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

TEST_CASE("materialize", "[execution]")
{
    SECTION("value")
    {
        SECTION("none")
        {
            const auto res = tt::sync_wait(ex::just() | ex::materialize());
            REQUIRE(res);
            const auto& [set_value] = *res;
            STATIC_REQUIRE(std::same_as<std::decay_t<decltype(set_value)>, ex::set_value_t>);
        }
        SECTION("one")
        {
            const auto res = tt::sync_wait(ex::just(42) | ex::materialize());
            REQUIRE(res);
            const auto& [set_value, i] = *res;
            STATIC_REQUIRE(std::same_as<std::decay_t<decltype(set_value)>, ex::set_value_t>);
            REQUIRE(i == 42);
        }
        SECTION("many")
        {
            const auto res = tt::sync_wait(ex::just("Hello!"s, 42) | ex::materialize());
            REQUIRE(res);
            const auto& [set_value, s, i] = *res;
            STATIC_REQUIRE(std::same_as<std::decay_t<decltype(set_value)>, ex::set_value_t>);
            REQUIRE(s == "Hello!");
            REQUIRE(i == 42);
        }
    }
    SECTION("error")
    {
        const auto res = tt::sync_wait(ex::just_error(42) | ex::materialize());
        REQUIRE(res);
        const auto& [set_error, err] = *res;
        STATIC_REQUIRE(std::same_as<std::decay_t<decltype(set_error)>, ex::set_error_t>);
        REQUIRE(err == 42);
    }
    SECTION("stopped")
    {
        const auto res = tt::sync_wait(ex::stop() | ex::materialize());
        REQUIRE(res);
        const auto& [set_stopped] = *res;
        STATIC_REQUIRE(std::same_as<std::decay_t<decltype(set_stopped)>, ex::set_stopped_t>);
    }
}

TEST_CASE("dematerialize", "[execution]")
{
    SECTION("value")
    {
        SECTION("none")
        {
            const auto res = tt::sync_wait(ex::just(ex::set_value) | ex::dematerialize());
            REQUIRE(res);
        }
        SECTION("one")
        {
            const auto res = tt::sync_wait(ex::just(ex::set_value, 42) | ex::dematerialize());
            REQUIRE(res);
            const auto [i] = *res;
            REQUIRE(i == 42);
        }
        SECTION("many")
        {
            const auto res = tt::sync_wait(ex::just(ex::set_value, "Hello!"s, 42) | ex::dematerialize());
            REQUIRE(res);
            const auto& [s, i] = *res;
            REQUIRE(s == "Hello!");
            REQUIRE(i == 42);
        }
    }
    SECTION("error")
    {
        try
        {
            tt::sync_wait_with_variant(ex::just(ex::set_error, 42) | ex::dematerialize());
            FAIL(); // should not reach here
        }
        catch (const int e)
        {
            REQUIRE(e == 42);
        }
    }
    SECTION("stopped")
    {
        const auto res = tt::sync_wait_with_variant(ex::just(ex::set_stopped) | ex::dematerialize());
        REQUIRE_FALSE(res);
    }
}

TEST_CASE("let", "[execution]")
{
    SECTION("transform value")
    {
        const auto res = tt::sync_wait(ex::just(21) | ex::let_value([](const int v) { return ex::just(v * 2); }));
        REQUIRE(res);
        const auto [i] = *res;
        REQUIRE(i == 42);
    }
    SECTION("value to error")
    {
        try
        {
            tt::sync_wait_with_variant(ex::just(42) | ex::let_value([](const int v) { return ex::just_error(v); }));
            FAIL();
        }
        catch (const int i)
        {
            REQUIRE(i == 42);
        }
    }
    SECTION("value to stopped")
    {
        const auto res = tt::sync_wait_with_variant(ex::just() | ex::let_value([] { return ex::stop(); }));
        REQUIRE_FALSE(res);
    }
    SECTION("error propagates")
    {
        REQUIRE_THROWS_WITH( //
            tt::sync_wait(
                ex::just() | ex::let_value([]() -> decltype(ex::just()) { throw std::runtime_error("oh no"); })),
            "oh no");
    }
    SECTION("error to value")
    {
        const auto res = tt::sync_wait(ex::just_error(42) | ex::let_error([](const auto e) { return ex::just(e); }));
        REQUIRE(res);
        const auto [i] = *res;
        REQUIRE(i == 42);
    }
}

TEST_CASE("with query value", "[execution]")
{
    SECTION("basic")
    {
        const auto res = tt::sync_wait( //
            ex::with_query_value(ex::read(clu::get_stop_token), //
                clu::get_stop_token, clu::in_place_stop_token{}) //
        );
        REQUIRE(res);
        const auto [token] = *res;
        STATIC_REQUIRE(std::same_as<std::decay_t<decltype(token)>, clu::in_place_stop_token>);
        REQUIRE(!token.stop_possible());
    }
    SECTION("innermost value is active")
    {
        // clang-format off
        tt::sync_wait(
            ex::with_query_value(
                ex::with_query_value(
                    ex::read(clu::get_stop_token)
                    | ex::then(
                          [](const auto token)
                          {
                              STATIC_REQUIRE(std::same_as<std::decay_t<decltype(token)>, clu::in_place_stop_token>);
                              REQUIRE(!token.stop_possible());
                          }),
                    clu::get_stop_token, clu::in_place_stop_token{}
                )
                | ex::let_value([] { return ex::read(clu::get_stop_token); }) //
                | ex::then([](const auto token)
                      { STATIC_REQUIRE(std::same_as<std::decay_t<decltype(token)>, clu::never_stop_token>); }),
                clu::get_stop_token, clu::never_stop_token{}
            )
        );
        // clang-format on
    }
    SECTION("multiple values")
    {
        using alloc_t = std::pmr::polymorphic_allocator<>;
        const alloc_t alloc;
        // clang-format off
        tt::sync_wait(
            ex::with_query_value(
                ex::with_query_value(
                    ex::read(clu::get_stop_token) 
                    | ex::then(
                          [](const auto token)
                          {
                              STATIC_REQUIRE(std::same_as<std::decay_t<decltype(token)>, clu::in_place_stop_token>);
                              REQUIRE(!token.stop_possible());
                          }) //
                    | ex::let_value([] { return ex::read(clu::get_allocator); }) 
                    | ex::then([=](const auto a) { REQUIRE(a == alloc); }),
                    clu::get_stop_token, clu::in_place_stop_token{}
                ), clu::get_allocator, alloc
            ) 
        );
        // clang-format on
    }
}

TEST_CASE("into variant", "[execution]")
{
    enum outcome
    {
        none,
        some,
        err,
        stop
    };
    // TODO: we need a "variant sender" to make this better, this is too hacky
    // currently using the hack of throwing error and then transforming the error
    // using let_error to achieve the desired multiple value paths
    const auto task_fn = [](const outcome o) -> clu::task<void>
    {
        if (o == some)
            co_await ex::just_error(some);
        else if (o == stop)
            co_await ex::stop();
    };
    const auto factory = [&](const outcome o)
    {
        return task_fn(o) //
            | ex::upon_error([](auto) { return 42; }) // "some" branch
            | ex::let_value(clu::overload(
                  [o]
                  {
                      if (o == err)
                          throw std::runtime_error("oh no");
                      return ex::just();
                  },
                  [](const auto val) { return ex::just(val); } // don't touch the "some" path
                  )) //
            | ex::into_variant();
    };
    SECTION("none")
    {
        const auto res = tt::sync_wait(factory(none));
        REQUIRE(res);
        std::visit(
            []<typename T>(T)
            {
                if constexpr (std::tuple_size_v<T> == 0)
                    SUCCEED();
                else // "some", which is wrong
                    FAIL();
            },
            std::get<0>(*res));
    }
    SECTION("some")
    {
        const auto res = tt::sync_wait(factory(some));
        REQUIRE(res);
        std::visit(
            []<typename T>(const T& tup)
            {
                if constexpr (std::tuple_size_v<T> == 0) // "none", which is wrong
                    FAIL();
                else
                {
                    const auto [value] = tup;
                    REQUIRE(value == 42);
                }
            },
            std::get<0>(*res));
    }
    SECTION("err") { REQUIRE_THROWS_WITH(tt::sync_wait(factory(err)), "oh no"); }
    SECTION("stop")
    {
        const auto res = tt::sync_wait(factory(stop));
        REQUIRE_FALSE(res);
    }
}
