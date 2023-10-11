#include <vector>
#include <array>
#include <span>
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "clu/box.h"
#include "clu/memory_resource.h"
#include "tracking_memory_resource.h"

namespace ct = clu::testing;

struct S
{
    int value = 0;
    S() noexcept = default;
    explicit S(const int v) noexcept: value(v) {}
    virtual ~S() noexcept = default;
    S(S&&) noexcept = default;
};

struct T final : S
{
    using S::S;
};

auto&& get_value(auto&& v) noexcept
{
    if constexpr (requires { v.value; })
        return v.value;
    else
        return v;
}

void fill_iota(auto&& span)
{
    for (std::size_t i = 0; i < span.size(); i++)
        ::get_value(span[i]) = static_cast<int>(i);
}

bool is_iota(auto&& span)
{
    for (std::size_t i = 0; i < span.size(); i++)
        if (::get_value(span[i]) != i)
            return false;
    return true;
}

TEMPLATE_TEST_CASE("construct null boxes", "[box]", int, int[], S, T)
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("default allocator")
    {
        clu::box<TestType> box = clu::nullbox;
        CHECK_FALSE(box);
        CHECK(mem.bytes_allocated() == 0);
    }

    SECTION("given allocator")
    {
        auto box = clu::allocate_box<TestType>(alloc, clu::nullbox);
        CHECK_FALSE(box);
        CHECK(box.get_allocator() == alloc);
        CHECK(mem.bytes_allocated() == 0);
    }
}

TEMPLATE_TEST_CASE("construct boxes with simple types", "[box]", int, S, T)
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("single")
    {
        auto box = clu::allocate_box<TestType>(alloc, 42);
        CHECK(mem.bytes_allocated() == sizeof(TestType));
        CHECK(box.get_allocator() == alloc);
        CHECK(::get_value(*box) == 42);
    }

    SECTION("array")
    {
        auto box = clu::allocate_box<TestType[]>(alloc, 12);
        CHECK(mem.bytes_allocated() == 12 * sizeof(TestType));
        CHECK(box.size() == 12);
        CHECK(box.get_allocator() == alloc);
        const auto all_zero = std::ranges::all_of(box, [](const auto& v) { return get_value(v) == 0; });
        CHECK(all_zero);
    }

    CHECK(mem.bytes_allocated() == 0);
}

TEST_CASE("allocator propagating box constructor", "[box]")
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);
    using vector = std::pmr::vector<int>;

    SECTION("single")
    {
        auto box = clu::allocate_box<vector>(alloc);
        const auto vector_alloc = mem.bytes_allocated();
        CHECK(box.get_allocator() == alloc);
        CHECK(box->get_allocator() == alloc);
        box->reserve(24);
        CHECK(mem.bytes_allocated() == vector_alloc + 24 * sizeof(int));
    }

    SECTION("array")
    {
        auto box = clu::allocate_box<vector[]>(alloc, 17);
        CHECK(box.size() == 17);
        const bool correct_alloc =
            std::ranges::all_of(box, [&](const vector& v) { return v.get_allocator() == alloc; });
        CHECK(correct_alloc);
    }

    CHECK(mem.bytes_allocated() == 0);
}

TEMPLATE_TEST_CASE("move construct box", "[box]", int, S, T)
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("single")
    {
        auto b1 = clu::allocate_box<TestType>(alloc, 42);
        CHECK(b1.get_allocator() == alloc);
        auto b2 = std::move(b1);
        CHECK(b2.get_allocator() == alloc);
        CHECK(::get_value(*b2) == 42);
        CHECK(mem.bytes_allocated() == sizeof(TestType));
    }

    SECTION("array")
    {
        auto b1 = clu::allocate_box<TestType[]>(alloc, 16);
        ::fill_iota(b1);
        CHECK(b1.get_allocator() == alloc);
        auto b2 = std::move(b1);
        CHECK(b2.get_allocator() == alloc);
        CHECK(::is_iota(b1));
        CHECK(mem.bytes_allocated() == 16 * sizeof(TestType));
    }

    CHECK(mem.bytes_allocated() == 0);
}

using types = clu::meta::cartesian_product<
    clu::type_list<std::pmr::polymorphic_allocator<>, clu::propagating_polymorphic_allocator<>>,
    clu::type_list<int, T>>;

TEMPLATE_LIST_TEST_CASE("move construct box with allocator replacement", "[box]", types)
{
    using Alloc = clu::meta::nth_type_l<TestType, 0>;
    using Type = clu::meta::nth_type_l<TestType, 1>;
    ct::tracking_memory_resource mem1, mem2;
    Alloc a1(&mem1), a2(&mem2);
    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);

    SECTION("single, switching allocator")
    {
        auto b1 = clu::allocate_box<Type>(a1, 42);
        CHECK(b1.get_allocator() == a1);
        decltype(b1) b2(std::allocator_arg, a2, std::move(b1));
        CHECK(b2.get_allocator() == a2);
        CHECK(mem2.bytes_allocated() == sizeof(Type));
        CHECK(::get_value(*b2) == 42);
    }

    SECTION("single, same allocator")
    {
        auto b1 = clu::allocate_box<Type>(a1, 42);
        CHECK(b1.get_allocator() == a1);
        decltype(b1) b2(std::allocator_arg, a1, std::move(b1));
        CHECK(b2.get_allocator() == a1);
        CHECK(mem1.bytes_allocated() == sizeof(Type));
        CHECK(mem2.bytes_allocated() == 0);
        CHECK(::get_value(*b2) == 42);
    }

    SECTION("array, switching allocator")
    {
        auto b1 = clu::allocate_box<Type[]>(a1, 12);
        CHECK(b1.get_allocator() == a1);
        ::fill_iota(b1);
        decltype(b1) b2(std::allocator_arg, a2, std::move(b1));
        CHECK(b2.get_allocator() == a2);
        CHECK(mem2.bytes_allocated() == sizeof(Type) * 12);
        CHECK(b2.size() == 12);
        CHECK(::is_iota(b2));
    }

    SECTION("array, same allocator")
    {
        auto b1 = clu::allocate_box<Type[]>(a1, 12);
        CHECK(b1.get_allocator() == a1);
        ::fill_iota(b1);
        decltype(b1) b2(std::allocator_arg, a1, std::move(b1));
        CHECK(b2.get_allocator() == a1);
        CHECK(mem1.bytes_allocated() == sizeof(Type) * 12);
        CHECK(mem2.bytes_allocated() == 0);
        CHECK(b2.size() == 12);
        CHECK(::is_iota(b2));
    }

    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);
}

TEMPLATE_TEST_CASE("allocator propagating box move constructor", "[box]", //
    std::pmr::polymorphic_allocator<>, clu::propagating_polymorphic_allocator<>)
{
    using vector = std::vector<int, clu::rebound_allocator_t<TestType, int>>;
    ct::tracking_memory_resource mem1, mem2;
    TestType a1(&mem1), a2(&mem2);
    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);

    SECTION("switching allocator")
    {
        auto b1 = clu::allocate_box<vector>(a1);
        CHECK(b1.get_allocator() == a1);
        const auto allocated = mem1.bytes_allocated();
        decltype(b1) b2(std::allocator_arg, a2, std::move(b1));
        CHECK(b2.get_allocator() == a2);
        CHECK(b2->get_allocator() == a2);
        CHECK(mem2.bytes_allocated() == allocated);
        b2->reserve(12);
        CHECK(mem2.bytes_allocated() == allocated + sizeof(int) * 12);
    }

    SECTION("same allocator")
    {
        auto b1 = clu::allocate_box<vector>(a1);
        CHECK(b1.get_allocator() == a1);
        const auto allocated = mem1.bytes_allocated();
        decltype(b1) b2(std::allocator_arg, a1, std::move(b1));
        CHECK(b2.get_allocator() == a1);
        CHECK(b2->get_allocator() == a1);
        CHECK(mem1.bytes_allocated() == allocated);
        b2->reserve(12);
        CHECK(mem1.bytes_allocated() == allocated + sizeof(int) * 12);
    }

    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);
}

TEMPLATE_TEST_CASE("resetting boxes", "[box]", int, S, T)
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("single")
    {
        auto box = clu::allocate_box<TestType>(alloc, 42);
        CHECK(mem.bytes_allocated() == sizeof(TestType));
        box.reset();
        CHECK(box.get_allocator() == alloc);
        CHECK_FALSE(box);
        CHECK(mem.bytes_allocated() == 0);
    }

    SECTION("array")
    {
        auto box = clu::allocate_box<TestType[]>(alloc, 12);
        CHECK(mem.bytes_allocated() == 12 * sizeof(TestType));
        box.reset();
        CHECK(box.get_allocator() == alloc);
        CHECK_FALSE(box);
        CHECK(mem.bytes_allocated() == 0);
    }

    CHECK(mem.bytes_allocated() == 0);
}

TEST_CASE("construct polymorphic box", "[box]")
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("none")
    {
        clu::pmr::box<S> box = clu::allocate_box<T>(alloc, clu::nullbox);
        CHECK(mem.bytes_allocated() == 0);
        CHECK_FALSE(box);
    }

    SECTION("some")
    {
        clu::pmr::box<S> box = clu::allocate_box<T>(alloc, 42);
        CHECK(mem.bytes_allocated() == sizeof(T));
        CHECK(box->value == 42);
    }

    CHECK(mem.bytes_allocated() == 0);
}

TEMPLATE_TEST_CASE("emplace into boxes", "[box]", int, S, T)
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("none")
    {
        auto box = clu::allocate_box<TestType>(alloc, clu::nullbox);
        box.emplace(42);
        CHECK(mem.bytes_allocated() == sizeof(TestType));
        CHECK(get_value(*box) == 42);
    }

    SECTION("some")
    {
        auto box = clu::allocate_box<TestType>(alloc, 16);
        box.emplace(42);
        CHECK(mem.bytes_allocated() == sizeof(TestType));
        CHECK(get_value(*box) == 42);
    }

    CHECK(mem.bytes_allocated() == 0);
}

TEST_CASE("emplace into polymorphic box", "[box]")
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("none")
    {
        auto box = clu::allocate_box<S>(alloc, clu::nullbox);
        box.emplace<T>(42);
        CHECK(mem.bytes_allocated() == sizeof(T));
        CHECK(get_value(*box) == 42);
    }

    SECTION("some")
    {
        auto box = clu::allocate_box<S>(alloc, 16);
        CHECK(mem.bytes_allocated() == sizeof(S));
        box.emplace<T>(42);
        CHECK(mem.bytes_allocated() == sizeof(T));
        CHECK(get_value(*box) == 42);
    }

    CHECK(mem.bytes_allocated() == 0);
}

TEMPLATE_LIST_TEST_CASE("move assign box", "[box]", types)
{
    using Alloc = clu::meta::nth_type_l<TestType, 0>;
    using Type = clu::meta::nth_type_l<TestType, 1>;
    constexpr bool pocma = std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value;
    ct::tracking_memory_resource mem1, mem2;
    Alloc a1(&mem1), a2(&mem2);
    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);

    SECTION("single, switching allocator")
    {
        auto b1 = clu::allocate_box<Type>(a1, 42);
        auto b2 = clu::allocate_box<Type>(a2, 21);
        b2 = std::move(b1);
        CHECK(b2.get_allocator() == (pocma ? a1 : a2));
        CHECK(::get_value(*b2) == 42);
    }

    SECTION("single, same allocator")
    {
        auto b1 = clu::allocate_box<Type>(a1, 42);
        auto b2 = clu::allocate_box<Type>(a1, 21);
        b2 = std::move(b1);
        CHECK(b2.get_allocator() == a1);
        CHECK(::get_value(*b2) == 42);
    }

    SECTION("array, switching allocator")
    {
        auto b1 = clu::allocate_box<Type[]>(a1, 12);
        ::fill_iota(b1);
        auto b2 = clu::allocate_box<Type[]>(a2, 12);
        b2 = std::move(b1);
        CHECK(b2.get_allocator() == (pocma ? a1 : a2));
        CHECK(::is_iota(b2));
    }

    SECTION("array, same allocator")
    {
        auto b1 = clu::allocate_box<Type[]>(a1, 12);
        ::fill_iota(b1);
        auto b2 = clu::allocate_box<Type[]>(a1, 12);
        b2 = std::move(b1);
        CHECK(b2.get_allocator() == a1);
        CHECK(::is_iota(b2));
    }

    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);
}

TEST_CASE("move assign polymorphic box", "[box]")
{
    ct::tracking_memory_resource mem1, mem2;
    clu::propagating_polymorphic_allocator<> a1(&mem1), a2(&mem2);
    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);

    SECTION("none")
    {
        auto s = clu::allocate_box<S>(a1, 21);
        auto t = clu::allocate_box<T>(a2, clu::nullbox);
        s = std::move(t);
        CHECK(s.get_allocator() == a2);
        CHECK_FALSE(s);
    }

    SECTION("some")
    {
        auto s = clu::allocate_box<S>(a1, 21);
        auto t = clu::allocate_box<T>(a2, 42);
        s = std::move(t);
        CHECK(s.get_allocator() == a2);
        CHECK(s->value == 42);
    }

    CHECK(mem1.bytes_allocated() == 0);
    CHECK(mem2.bytes_allocated() == 0);
}

TEMPLATE_TEST_CASE("box to shared", "[box]", int, S, T)
{
    ct::tracking_memory_resource mem;
    std::pmr::polymorphic_allocator alloc(&mem);
    CHECK(mem.bytes_allocated() == 0);

    SECTION("single")
    {
        auto box = clu::allocate_box<TestType>(alloc, 42);
        CHECK(mem.bytes_allocated() == sizeof(TestType));
        auto shared = std::move(box).to_shared();
        box = clu::nullbox;
        CHECK(::get_value(*shared) == 42);
        CHECK(mem.bytes_allocated() > sizeof(TestType)); // Control block
    }

    SECTION("array")
    {
        constexpr auto size = 12;
        auto box = clu::allocate_box<TestType[]>(alloc, size);
        ::fill_iota(box);
        CHECK(mem.bytes_allocated() == size * sizeof(TestType));
        auto shared = std::move(box).to_shared();
        box = clu::nullbox;
        CHECK(::is_iota(std::span(shared.get(), size)));
        CHECK(mem.bytes_allocated() > size * sizeof(TestType)); // Control block
    }

    CHECK(mem.bytes_allocated() == 0);
}
