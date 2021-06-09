#include <gtest/gtest.h>
#include <clu/string_utils.h>

using namespace std::literals;

TEST(StringUtils, StrLen)
{
    EXPECT_EQ(clu::strlen("hello world"), 11);
    EXPECT_EQ(clu::strlen("hello world"s), 11);
    EXPECT_EQ(clu::strlen("hello world"sv), 11);
}

TEST(StringUtils, ReplaceAll)
{
    using sv = std::string_view;

    const auto copy_as_string = [](const sv source, const sv from, const sv to)
    {
        std::string result;
        clu::replace_all_copy(std::back_inserter(result), source, from, to);
        return result;
    };
    EXPECT_EQ(copy_as_string("hello world", "world", "warudo"), "hello warudo");
    EXPECT_EQ(copy_as_string("aabbaacc", "aa", "dd"), "ddbbddcc");
    EXPECT_EQ(copy_as_string("aaaaaaaa", "aaa", "abcd"), "abcdabcdaa");

    const auto as_string = [](const sv source, const sv from, const sv to)
    {
        std::string result{ source };
        clu::replace_all(result, from, to);
        return result;
    };
    EXPECT_EQ(as_string("hello world", "world", "warudo"), "hello warudo");
    EXPECT_EQ(as_string("aabbaacc", "aa", "dd"), "ddbbddcc");
    EXPECT_EQ(as_string("aaaaaaaa", "aaa", "abcd"), "abcdabcdaa");
    EXPECT_EQ(as_string("aaaaaaaa", "aaa", "ab"), "ababaa");
}
