#include <catch2/catch.hpp>
#include <clu/meta_algorithm.h>

using namespace clu;
using namespace meta;

template <typename T> using size_of_four = std::bool_constant<sizeof(T) == 4>;

TEST_CASE("meta find", "[meta_algorithm]")
{
    static_assert(type_list<double, int, float, void>::apply_v<find<float>::apply> == 2);
    static_assert(type_list<double, int, float, void>::apply_v<find<char>::apply> == npos);
}

TEST_CASE("meta filter", "[meta_algorithm]")
{
    static_assert(std::is_same_v<
        type_list<uint32_t, double, int32_t>::apply_t<filter<size_of_four>::apply>,
        type_list<uint32_t, int32_t>>);
}

TEST_CASE("meta is_unique", "[meta_algorithm]")
{
    static_assert(type_list<>::apply_v<is_unique>);
    static_assert(type_list<int>::apply_v<is_unique>);
    static_assert(!type_list<int, int>::apply_v<is_unique>);
}

TEST_CASE("meta set_equal", "[meta_algorithm]")
{
    static_assert(set_equal_v<type_list<int, float>, type_list<float, int>>);
    static_assert(!set_equal_v<type_list<int>, type_list<float, int>>);
    static_assert(set_equal_v<type_list<>, type_list<>>);
}

TEST_CASE("meta unique", "[meta_algorithm]")
{
    static_assert(set_equal_v<type_list<int, float, int>::apply_t<unique>, type_list<float, int>>);
    static_assert(set_equal_v<type_list<>::apply_t<unique>, type_list<>>);
    static_assert(!set_equal_v<type_list<void>::apply_t<unique>, type_list<>>);
}

TEST_CASE("meta set_include", "[meta_algorithm]")
{
    static_assert(set_include_v<type_list<int, float, double>, type_list<float, double>>);
    static_assert(!set_include_v<type_list<int, float, double>, type_list<float, int, void>>);
}

TEST_CASE("meta set operations", "[meta_algorithm]")
{
    static_assert(set_equal_v<
        set_union_t<type_list<int, float, double>, type_list<void, int, float>>,
        type_list<int, void, double, float>>);
    static_assert(set_equal_v<
        set_intersection_t<type_list<int, float, double>, type_list<void, int, float>>,
        type_list<int, float>>);
    static_assert(set_equal_v<
        set_difference_t<type_list<int, float, double>, type_list<void, int, float>>,
        type_list<double>>);
    static_assert(set_equal_v<
        set_symmetric_difference_t<type_list<int, float, double>, type_list<void, int, float>>,
        type_list<void, double>>);
}
