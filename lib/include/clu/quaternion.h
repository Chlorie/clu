#pragma once

#include <cmath>

#include "concepts.h"

namespace clu
{
    template <typename T>
    class quaternion
    {
    public:
        using value_type = T;
        using param_type = conditional_t<std::is_floating_point_v<T>, T, const T&>;

        constexpr quaternion() noexcept = default;

        constexpr explicit(false) quaternion(const param_type x) noexcept: data_{x} {}

        constexpr quaternion(const param_type x, const param_type y, const param_type z, const param_type w) noexcept:
            data_{x, y, z, w}
        {
        }

        constexpr quaternion(const quaternion&) noexcept = default;

        template <typename U>
        constexpr explicit(is_narrowing_conversion_v<U, T>) quaternion(const quaternion<U>& other) noexcept:
            quaternion(static_cast<T>(other.data_[0]), static_cast<T>(other.data_[1]), //
                static_cast<T>(other.data_[2]), static_cast<T>(other.data_[3]))
        {
        }

        constexpr quaternion& operator=(const quaternion&) noexcept = default;

        [[nodiscard]] constexpr friend bool operator==(const quaternion&, const quaternion&) noexcept = default;

        [[nodiscard]] constexpr T real() const noexcept { return data_[0]; }
        [[nodiscard]] constexpr T imagi() const noexcept { return data_[1]; }
        [[nodiscard]] constexpr T imagj() const noexcept { return data_[2]; }
        [[nodiscard]] constexpr T imagk() const noexcept { return data_[3]; }

        constexpr quaternion& operator+=(const param_type other) noexcept
        {
            data_[0] += other;
            return *this;
        }

        constexpr quaternion& operator+=(const quaternion& other) noexcept
        {
            data_[0] += other.data_[0];
            data_[1] += other.data_[1];
            data_[2] += other.data_[2];
            data_[3] += other.data_[3];
            return *this;
        }

        constexpr quaternion& operator-=(const param_type other) noexcept
        {
            data_[0] -= other;
            return *this;
        }

        constexpr quaternion& operator-=(const quaternion& other) noexcept
        {
            data_[0] -= other.data_[0];
            data_[1] -= other.data_[1];
            data_[2] -= other.data_[2];
            data_[3] -= other.data_[3];
            return *this;
        }

        constexpr quaternion& operator*=(const param_type other) noexcept
        {
            data_[0] *= other;
            data_[1] *= other;
            data_[2] *= other;
            data_[3] *= other;
            return *this;
        }

        constexpr quaternion& operator*=(const quaternion& other) noexcept { return *this = (*this * other); }

        constexpr quaternion& operator/=(const param_type other) noexcept
        {
            data_[0] /= other;
            data_[1] /= other;
            data_[2] /= other;
            data_[3] /= other;
            return *this;
        }

        [[nodiscard]] constexpr T* data() noexcept { return data_; }
        [[nodiscard]] constexpr const T* data() const noexcept { return data_; }

        [[nodiscard]] constexpr friend quaternion operator+(const quaternion& quat) noexcept { return quat; }

        [[nodiscard]] constexpr friend quaternion operator-(const quaternion& quat) noexcept
        {
            return {quat.data_[0], quat.data_[1], quat.data_[2], quat.data_[3]};
        }

        [[nodiscard]] constexpr friend quaternion operator+(const param_type lhs, const quaternion& rhs) noexcept
        {
            return {
                lhs + rhs.data_[0], //
                rhs.data_[1], //
                rhs.data_[2], //
                rhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator+(const quaternion& lhs, const param_type rhs) noexcept
        {
            return {
                lhs.data_[0] + rhs, //
                lhs.data_[1], //
                lhs.data_[2], //
                lhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator+(const quaternion& lhs, const quaternion& rhs) noexcept
        {
            return {
                lhs.data_[0] + rhs.data_[0], //
                lhs.data_[1] + rhs.data_[1], //
                lhs.data_[2] + rhs.data_[2], //
                lhs.data_[3] + rhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator-(const param_type lhs, const quaternion& rhs) noexcept
        {
            return {
                lhs - rhs.data_[0], //
                -rhs.data_[1], //
                -rhs.data_[2], //
                -rhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator-(const quaternion& lhs, const param_type rhs) noexcept
        {
            return {
                lhs.data_[0] - rhs, //
                lhs.data_[1], //
                lhs.data_[2], //
                lhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator-(const quaternion& lhs, const quaternion& rhs) noexcept
        {
            return {
                lhs.data_[0] - rhs.data_[0], //
                lhs.data_[1] - rhs.data_[1], //
                lhs.data_[2] - rhs.data_[2], //
                lhs.data_[3] - rhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator*(const param_type lhs, const quaternion& rhs) noexcept
        {
            return {
                lhs * rhs.data_[0], //
                lhs * rhs.data_[1], //
                lhs * rhs.data_[2], //
                lhs * rhs.data_[3] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator*(const quaternion& lhs, const param_type rhs) noexcept
        {
            return {
                lhs.data_[0] * rhs, //
                lhs.data_[1] * rhs, //
                lhs.data_[2] * rhs, //
                lhs.data_[3] * rhs //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator*(const quaternion& lhs, const quaternion& rhs) noexcept
        {
            return {
                lhs.data_[0] * rhs.data_[0] - lhs.data_[1] * rhs.data_[1] //
                    - lhs.data_[2] * rhs.data_[2] - lhs.data_[3] * rhs.data_[3],
                lhs.data_[0] * rhs.data_[1] + lhs.data_[1] * rhs.data_[0] //
                    + lhs.data_[2] * rhs.data_[3] - lhs.data_[3] * rhs.data_[2],
                lhs.data_[0] * rhs.data_[2] + lhs.data_[2] * rhs.data_[0] //
                    + lhs.data_[3] * rhs.data_[1] - lhs.data_[1] * rhs.data_[3],
                lhs.data_[0] * rhs.data_[3] + lhs.data_[3] * rhs.data_[0] //
                    + lhs.data_[1] * rhs.data_[2] - lhs.data_[2] * rhs.data_[1] //
            };
        }

        [[nodiscard]] constexpr friend quaternion operator/(const param_type lhs, const quaternion& rhs) noexcept
        {
            const T scale = lhs / rhs.norm();
            return scale * rhs.conj();
        }

        [[nodiscard]] constexpr friend quaternion operator/(const quaternion& lhs, const param_type rhs) noexcept
        {
            return {
                lhs.data_[0] / rhs, //
                lhs.data_[1] / rhs, //
                lhs.data_[2] / rhs, //
                lhs.data_[3] / rhs //
            };
        }

        [[nodiscard]] constexpr quaternion conj() const noexcept { return {data_[0], -data_[1], -data_[2], -data_[3]}; }

        [[nodiscard]] constexpr T norm() const noexcept
        {
            return data_[0] * data_[0] + data_[1] * data_[1] //
                + data_[2] * data_[2] + data_[3] * data_[3];
        }

        [[nodiscard]] T abs() const noexcept
        {
            using std::sqrt;
            return sqrt(norm());
        }

        [[nodiscard]] quaternion versor() const noexcept { return *this / abs(); }

        [[nodiscard]] constexpr quaternion recip() const noexcept { return conj() / norm(); }

    private:
        value_type data_[4]{};
    };

#define CLU_QUAT_COMP(comp)                                                                                            \
    template <typename T>                                                                                              \
    [[nodiscard]] constexpr T comp(const quaternion<T>& quat) noexcept                                                 \
    {                                                                                                                  \
        return quat.comp();                                                                                            \
    }                                                                                                                  \
    static_assert(true)

    CLU_QUAT_COMP(real);
    CLU_QUAT_COMP(imagi);
    CLU_QUAT_COMP(imagj);
    CLU_QUAT_COMP(imagk);

#undef CLU_QUAT_COMP

    template <typename T>
    [[nodiscard]] constexpr quaternion<T> conj(const quaternion<T>& quat) noexcept
    {
        return quat.conj();
    }

    template <typename T>
    [[nodiscard]] constexpr T norm(const quaternion<T>& quat) noexcept
    {
        return quat.norm();
    }

    template <typename T>
    [[nodiscard]] T abs(const quaternion<T>& quat) noexcept
    {
        return quat.abs();
    }

    template <typename T>
    [[nodiscard]] quaternion<T> versor(const quaternion<T>& quat) noexcept
    {
        return quat.versor();
    }

    template <typename T>
    [[nodiscard]] constexpr quaternion<T> recip(const quaternion<T>& quat) noexcept
    {
        return quat.recip();
    }

    inline namespace literals
    {
        inline namespace quaternion_literals
        {
#define CLU_QUAT_LIT_I(R, T, suffix)                                                                                   \
    [[nodiscard]] constexpr quaternion<R> operator""_i##suffix(const T value) noexcept                                 \
    {                                                                                                                  \
        return quaternion<R>(R{}, static_cast<R>(value), R{}, R{});                                                    \
    }                                                                                                                  \
    static_assert(true)

#define CLU_QUAT_LIT_J(R, T, suffix)                                                                                   \
    [[nodiscard]] constexpr quaternion<R> operator""_j##suffix(const T value) noexcept                                 \
    {                                                                                                                  \
        return quaternion<R>(R{}, R{}, static_cast<R>(value), R{});                                                    \
    }                                                                                                                  \
    static_assert(true)

#define CLU_QUAT_LIT_K(R, T, suffix)                                                                                   \
    [[nodiscard]] constexpr quaternion<R> operator""_k##suffix(const T value) noexcept                                 \
    {                                                                                                                  \
        return quaternion<R>(R{}, R{}, R{}, static_cast<R>(value));                                                    \
    }                                                                                                                  \
    static_assert(true)

#define CLU_QUAT_LIT(macro)                                                                                            \
    macro(float, long double, f);                                                                                      \
    macro(float, unsigned long long, f);                                                                               \
    macro(double, long double, );                                                                                      \
    macro(double, unsigned long long, );                                                                               \
    macro(long double, long double, l);                                                                                \
    macro(long double, unsigned long long, l)

            CLU_QUAT_LIT(CLU_QUAT_LIT_I);
            CLU_QUAT_LIT(CLU_QUAT_LIT_J);
            CLU_QUAT_LIT(CLU_QUAT_LIT_K);

#undef CLU_QUAT_LIT
#undef CLU_QUAT_LIT_I
#undef CLU_QUAT_LIT_J
#undef CLU_QUAT_LIT_K
        } // namespace quaternion_literals
    } // namespace literals
} // namespace clu
