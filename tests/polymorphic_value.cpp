#include <numbers>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "clu/polymorphic_value.h"

using namespace Catch::literals;
using Catch::Approx;

class Shape
{
public:
    virtual ~Shape() noexcept = default;
    virtual float area() const noexcept = 0;
};

class Rectangle final : public Shape
{
private:
    float width_ = 0.0f, height_ = 0.0f;

public:
    Rectangle(const float width, const float height): width_(width), height_(height) {}
    float area() const noexcept override { return width_ * height_; }
};

class Disk final : public Shape
{
private:
    float radius_ = 0.0f;

public:
    explicit Disk(const float radius): radius_(radius) {}
    float area() const noexcept override { return std::numbers::pi_v<float> * radius_ * radius_; }
};

using ShapeValue = clu::polymorphic_value<Shape>;
using RectValue = clu::polymorphic_value<Rectangle>;
using DiskValue = clu::polymorphic_value<Disk>;

TEST_CASE("polymorphic value constructors", "[polymorphic_value]")
{
    SECTION("default")
    {
        const ShapeValue shape;
        REQUIRE(!shape);
    }
    SECTION("raw derived pointer")
    {
        const ShapeValue rect(new Rectangle(3.0f, 4.0f));
        REQUIRE(rect);
        const auto& ref = *rect;
        REQUIRE(typeid(ref) == typeid(Rectangle));
        REQUIRE(rect->area() == 12.0_a);
    }
    SECTION("raw base pointer")
    {
        Shape* ptr = new Rectangle(4.0f, 5.0f);
        const ShapeValue rect(
            ptr, [](const Shape& s) -> Shape* { return new Rectangle(dynamic_cast<const Rectangle&>(s)); });
        REQUIRE(rect);
        const auto& ref = *rect;
        REQUIRE(typeid(ref) == typeid(Rectangle));
        REQUIRE(rect->area() == 20.0_a);
    }
    SECTION("in place")
    {
        const RectValue rect(std::in_place, 2.5f, 4.0f);
        REQUIRE(rect);
        const auto& ref1 = *rect;
        REQUIRE(typeid(ref1) == typeid(Rectangle));
        REQUIRE(rect->area() == 10.0_a);

        const ShapeValue shape(std::in_place_type<Rectangle>, 2.0f, 2.5f);
        REQUIRE(shape);
        const auto& ref2 = *rect;
        REQUIRE(typeid(ref2) == typeid(Rectangle));
        REQUIRE(shape->area() == 5.0_a);
    }
    SECTION("value conversion")
    {
        const RectValue rect = Rectangle(2.5f, 4.0f);
        REQUIRE(rect);
        const auto& ref1 = *rect;
        REQUIRE(typeid(ref1) == typeid(Rectangle));
        REQUIRE(rect->area() == 10.0_a);

        const ShapeValue shape = Rectangle(2.0f, 2.5f);
        REQUIRE(shape);
        const auto& ref2 = *rect;
        REQUIRE(typeid(ref2) == typeid(Rectangle));
        REQUIRE(shape->area() == 5.0_a);
    }
    SECTION("copy")
    {
        const ShapeValue disk(std::in_place_type<Disk>, 3.0f);
        const ShapeValue copy = disk; // NOLINT(performance-unnecessary-copy-initialization)
        REQUIRE(copy);
        const auto& ref = *copy;
        REQUIRE(typeid(ref) == typeid(Disk));
        REQUIRE(&*disk != &*copy); // Deep copy
        REQUIRE(disk->area() == Approx(copy->area()));
    }
    SECTION("copy and conversion")
    {
        const DiskValue disk(std::in_place, 2.5f);
        const ShapeValue copy = disk;
        REQUIRE(copy);
        const auto& ref = *copy;
        REQUIRE(typeid(ref) == typeid(Disk));
        REQUIRE(&*disk != &*copy);
        REQUIRE(disk->area() == Approx(copy->area()));
    }
}

// TODO: more tests
