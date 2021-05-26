#include <gtest/gtest.h>
#include <numbers>

#include "clu/polymorphic_value.h"

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

TEST(PolymorphicValueCtor, Default)
{
    const ShapeValue shape;
    EXPECT_TRUE(!shape);
}

TEST(PolymorphicValueCtor, RawDerivedPointer)
{
    const ShapeValue rect(new Rectangle(3.0f, 4.0f));
    ASSERT_TRUE(rect);
    EXPECT_TRUE(typeid(*rect) == typeid(Rectangle));
    EXPECT_FLOAT_EQ(rect->area(), 12.0f);
}

TEST(PolymorphicValueCtor, RawBasePointer)
{
    Shape* ptr = new Rectangle(4.0f, 5.0f);
    const ShapeValue rect(ptr,
        [](const Shape& s)-> Shape* { return new Rectangle(dynamic_cast<const Rectangle&>(s)); });
    ASSERT_TRUE(rect);
    EXPECT_TRUE(typeid(*rect) == typeid(Rectangle));
    EXPECT_FLOAT_EQ(rect->area(), 20.0f);
}

TEST(PolymorphicValueCtor, Inplace)
{
    const RectValue rect(std::in_place, 2.5f, 4.0f);
    ASSERT_TRUE(rect);
    EXPECT_TRUE(typeid(*rect) == typeid(Rectangle));
    EXPECT_FLOAT_EQ(rect->area(), 10.0f);

    const ShapeValue shape(std::in_place_type<Rectangle>, 2.0f, 2.5f);
    ASSERT_TRUE(shape);
    EXPECT_TRUE(typeid(*shape) == typeid(Rectangle));
    EXPECT_FLOAT_EQ(shape->area(), 5.0f);
}

TEST(PolymorphicValueCtor, ValueConvert)
{
    const RectValue rect = Rectangle(2.5f, 4.0f);
    ASSERT_TRUE(rect);
    EXPECT_TRUE(typeid(*rect) == typeid(Rectangle));
    EXPECT_FLOAT_EQ(rect->area(), 10.0f);

    const ShapeValue shape = Rectangle(2.0f, 2.5f);
    ASSERT_TRUE(shape);
    EXPECT_TRUE(typeid(*shape) == typeid(Rectangle));
    EXPECT_FLOAT_EQ(shape->area(), 5.0f);
}

TEST(PolymorphicValueCtor, Copy)
{
    const ShapeValue disk(std::in_place_type<Disk>, 3.0f);
    const ShapeValue copy = disk; // NOLINT(performance-unnecessary-copy-initialization)
    ASSERT_TRUE(copy);
    EXPECT_TRUE(typeid(*copy) == typeid(Disk));
    EXPECT_NE(&*disk, &*copy); // Deep copy
    EXPECT_FLOAT_EQ(disk->area(), copy->area());
}

TEST(PolymorphicValueCtor, CopyConvert)
{
    const DiskValue disk(std::in_place, 2.5f);
    const ShapeValue copy = disk;
    ASSERT_TRUE(copy);
    EXPECT_TRUE(typeid(*copy) == typeid(Disk));
    EXPECT_NE(&*disk, &*copy);
    EXPECT_FLOAT_EQ(disk->area(), copy->area());
}

// TODO: more tests
