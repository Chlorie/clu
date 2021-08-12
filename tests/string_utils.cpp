#include <catch2/catch.hpp>
#include <clu/string_utils.h>

using namespace std::literals;

TEST_CASE("strlen", "[string_utils]")
{
    REQUIRE(clu::strlen("hello world") == 11);
    REQUIRE(clu::strlen("hello world"s) == 11);
    REQUIRE(clu::strlen("hello world"sv) == 11);
}

TEST_CASE("string replace", "[string_utils]")
{
    using sv = std::string_view;

    constexpr auto copy_as_string = [](const sv source, const sv from, const sv to)
    {
        std::string result;
        clu::replace_all_copy(std::back_inserter(result), source, from, to);
        return result;
    };
    REQUIRE(copy_as_string("hello world", "world", "warudo") == "hello warudo");
    REQUIRE(copy_as_string("aabbaacc", "aa", "dd") == "ddbbddcc");
    REQUIRE(copy_as_string("aaaaaaaa", "aaa", "abcd") == "abcdabcdaa");

    constexpr auto as_string = [](const sv source, const sv from, const sv to)
    {
        std::string result{ source };
        clu::replace_all(result, from, to);
        return result;
    };
    REQUIRE(as_string("hello world", "world", "warudo") == "hello warudo");
    REQUIRE(as_string("aabbaacc", "aa", "dd") == "ddbbddcc");
    REQUIRE(as_string("aaaaaaaa", "aaa", "abcd") == "abcdabcdaa");
    REQUIRE(as_string("aaaaaaaa", "aaa", "ab") == "ababaa");
}
