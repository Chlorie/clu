#include <catch2/catch_test_macros.hpp>
#include <array>

#include "clu/intrusive_list.h"
#include "matchers.h"

struct Elem : clu::intrusive_list_element_base<Elem>
{
    int value{};
    explicit Elem(const int val): value(val) {}
    friend bool operator==(const Elem lhs, const Elem rhs) noexcept { return lhs.value == rhs.value; }
    friend std::ostream& operator<<(std::ostream& os, const Elem elem) { return os << elem.value; }
};

namespace ct = clu::testing;

TEST_CASE("is bidirectional range", "[intrusive_list]")
{
    STATIC_REQUIRE(std::bidirectional_iterator<clu::intrusive_list<Elem>::iterator>);
    STATIC_REQUIRE(std::ranges::bidirectional_range<clu::intrusive_list<Elem>>);
}

TEST_CASE("push from ends", "[intrusive_list]")
{
    std::array elems{Elem(0), Elem(1), Elem(2), Elem(3)};
    clu::intrusive_list<Elem> list;

    SECTION("empty")
    {
        REQUIRE(list.empty());
        REQUIRE_THAT(list, ct::EqualsRange(std::initializer_list<Elem>{}));
    }

    SECTION("push_back")
    {
        list.push_back(elems[0]);
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0)}));
        list.push_back(elems[1]);
        list.push_back(elems[2]);
        list.push_back(elems[3]);
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2), Elem(3)}));
    }

    SECTION("push_front")
    {
        list.push_front(elems[0]);
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0)}));
        list.push_front(elems[1]);
        list.push_front(elems[2]);
        list.push_front(elems[3]);
        REQUIRE_THAT(list, ct::EqualsRange({Elem(3), Elem(2), Elem(1), Elem(0)}));
    }
}

TEST_CASE("pop from ends", "[intrusive_list]")
{
    std::array elems{Elem(0), Elem(1), Elem(2), Elem(3)};
    clu::intrusive_list<Elem> list;
    list.push_back(elems[0]);
    list.push_back(elems[1]);
    list.push_back(elems[2]);
    list.push_back(elems[3]);
    REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2), Elem(3)}));

    SECTION("pop_back")
    {
        list.pop_back();
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2)}));
        list.pop_back();
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1)}));
        list.pop_back();
        list.pop_back();
        REQUIRE(list.empty());
        REQUIRE_THAT(list, ct::EqualsRange(std::initializer_list<Elem>{}));
    }

    SECTION("pop_front")
    {
        list.pop_front();
        REQUIRE_THAT(list, ct::EqualsRange({Elem(1), Elem(2), Elem(3)}));
        list.pop_front();
        REQUIRE_THAT(list, ct::EqualsRange({Elem(2), Elem(3)}));
        list.pop_front();
        list.pop_front();
        REQUIRE(list.empty());
        REQUIRE_THAT(list, ct::EqualsRange(std::initializer_list<Elem>{}));
    }
}

TEST_CASE("insert", "[intrusive_list]")
{
    std::array elems{Elem(0), Elem(1), Elem(2), Elem(3)};
    clu::intrusive_list<Elem> list;
    list.push_back(elems[0]);
    list.push_back(elems[1]);
    REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1)}));

    SECTION("insert before begin")
    {
        const auto iter1 = list.insert(list.begin(), elems[2]);
        CHECK(iter1 == list.begin());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(2), Elem(0), Elem(1)}));
        const auto iter2 = list.insert(list.begin(), elems[3]);
        CHECK(iter2 == list.begin());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(3), Elem(2), Elem(0), Elem(1)}));
    }

    SECTION("insert before end")
    {
        const auto iter1 = list.insert(list.end(), elems[2]);
        CHECK(std::next(iter1) == list.end());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2)}));
        const auto iter2 = list.insert(list.end(), elems[3]);
        CHECK(std::next(iter2) == list.end());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2), Elem(3)}));
    }

    SECTION("insert in the middle")
    {
        const auto iter = std::next(list.begin());
        const auto iter1 = list.insert(iter, elems[2]);
        CHECK(std::next(iter1) == iter);
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(2), Elem(1)}));
        const auto iter2 = list.insert(iter, elems[3]);
        CHECK(std::next(iter1) == iter2);
        CHECK(std::next(iter2) == iter);
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(2), Elem(3), Elem(1)}));
    }
}

TEST_CASE("erase", "[intrusive_list]")
{
    std::array elems{Elem(0), Elem(1), Elem(2), Elem(3)};
    clu::intrusive_list<Elem> list;
    list.push_back(elems[0]);
    list.push_back(elems[1]);
    list.push_back(elems[2]);
    list.push_back(elems[3]);
    REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2), Elem(3)}));

    SECTION("erase begin")
    {
        const auto iter1 = list.erase(list.begin());
        CHECK(iter1 == list.begin());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(1), Elem(2), Elem(3)}));
        const auto iter2 = list.erase(list.begin());
        CHECK(iter2 == list.begin());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(2), Elem(3)}));
        list.erase(list.begin());
        list.erase(list.begin());
        REQUIRE(list.empty());
        REQUIRE_THAT(list, ct::EqualsRange(std::initializer_list<Elem>{}));
    }

    SECTION("erase last")
    {
        const auto iter1 = list.erase(std::prev(list.end()));
        CHECK(iter1 == list.end());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1), Elem(2)}));
        const auto iter2 = list.erase(std::prev(list.end()));
        CHECK(iter2 == list.end());
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(1)}));
        list.erase(std::prev(list.end()));
        list.erase(std::prev(list.end()));
        REQUIRE(list.empty());
        REQUIRE_THAT(list, ct::EqualsRange(std::initializer_list<Elem>{}));
    }

    SECTION("erase from the middle")
    {
        const auto iter1 = list.erase(std::next(list.begin()));
        CHECK(iter1 == std::next(list.begin()));
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(2), Elem(3)}));
        const auto iter2 = list.erase(std::next(list.begin()));
        CHECK(iter2 == std::next(list.begin()));
        REQUIRE_THAT(list, ct::EqualsRange({Elem(0), Elem(3)}));
    }
}
