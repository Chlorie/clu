#include <catch2/catch_test_macros.hpp>
#include "clu/meta_algorithm.h"

using namespace clu;
using namespace meta;

template <typename T>
using size_of_four = std::bool_constant<sizeof(T) == 4>;

TEST_CASE("meta find", "[meta_algorithm]")
{
    STATIC_REQUIRE(find_lv<type_list<double, int, float, void>, float> == 2);
    STATIC_REQUIRE(find_lv<type_list<double, int, float, void>, char> == npos);
}

TEST_CASE("meta filter", "[meta_algorithm]")
{
    STATIC_REQUIRE(std::is_same_v<filter_l<type_list<uint32_t, double, int32_t>, quote1<size_of_four>>,
        type_list<uint32_t, int32_t>>);
}

TEST_CASE("meta is_unique", "[meta_algorithm]")
{
    STATIC_REQUIRE(is_unique_lv<type_list<>>);
    STATIC_REQUIRE(is_unique_lv<type_list<int>>);
    STATIC_REQUIRE(!is_unique_lv<type_list<int, int>>);
}

TEST_CASE("meta set_equal", "[meta_algorithm]")
{
    STATIC_REQUIRE(set_equal_lv<type_list<int, float>, type_list<float, int>>);
    STATIC_REQUIRE(!set_equal_lv<type_list<int>, type_list<float, int>>);
    STATIC_REQUIRE(set_equal_lv<type_list<>, type_list<>>);
}

TEST_CASE("meta unique", "[meta_algorithm]")
{
    STATIC_REQUIRE(set_equal_lv<unique_l<type_list<int, float, int>>, type_list<float, int>>);
    STATIC_REQUIRE(set_equal_lv<unique_l<type_list<>>, type_list<>>);
    STATIC_REQUIRE(!set_equal_lv<unique_l<type_list<void>>, type_list<>>);
}

TEST_CASE("meta set_include", "[meta_algorithm]")
{
    STATIC_REQUIRE(set_include_lv<type_list<int, float, double>, type_list<float, double>>);
    STATIC_REQUIRE(!set_include_lv<type_list<int, float, double>, type_list<float, int, void>>);
}

TEST_CASE("meta set operations", "[meta_algorithm]")
{
    using L1 = type_list<int, float, double>;
    using L2 = type_list<void, int, float>;
    STATIC_REQUIRE(set_equal_lv<set_union_l<L1, L2>, type_list<int, void, double, float>>);
    STATIC_REQUIRE(set_equal_lv<set_intersection_l<L1, L2>, type_list<int, float>>);
    STATIC_REQUIRE(set_equal_lv<set_difference_l<L1, L2>, type_list<double>>);
    STATIC_REQUIRE(set_equal_lv<set_symmetric_difference_l<L1, L2>, type_list<void, double>>);
}
