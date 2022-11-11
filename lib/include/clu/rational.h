#pragma once

#include <concepts>
#include <numeric>
#include <compare>

namespace clu
{
    template <std::signed_integral T>
    class rational final
    {
    public:
        constexpr rational() noexcept = default;
        constexpr explicit(false) rational(const T value) noexcept: num_(value) {}
        constexpr rational(const T num, const T den) noexcept: num_(num), den_(den) { normalize(); }
        constexpr rational(const rational&) noexcept = default;

        // clang-format off
        template <std::signed_integral U>
            requires(!std::same_as<T, U>)
        constexpr explicit rational(const rational<U>& other) noexcept:
            rational(other.numerator(), other.denominator()) {}
        // clang-format on

        constexpr T numerator() const noexcept { return num_; }
        constexpr T denominator() const noexcept { return den_; }

        constexpr rational operator+() const noexcept { return *this; }
        constexpr rational operator-() const noexcept { return {-num_, den_}; }

        constexpr friend rational operator+(const rational lhs, const rational rhs) noexcept
        {
            const auto lcm = std::lcm(lhs.den_, rhs.den_);
            return {lhs.num_ * (lcm / lhs.den_) + rhs.num_ * (lcm / rhs.den_), lcm};
        }

        constexpr friend rational operator-(const rational lhs, const rational rhs) noexcept
        {
            const auto lcm = std::lcm(lhs.den_, rhs.den_);
            return {lhs.num_ * (lcm / lhs.den_) - rhs.num_ * (lcm / rhs.den_), lcm};
        }

        constexpr friend rational operator*(const rational lhs, const rational rhs) noexcept
        {
            return {lhs.num_ * rhs.num_, lhs.den_ * rhs.den_};
        }

        constexpr friend rational operator/(const rational lhs, const rational rhs) noexcept
        {
            return {lhs.num_ * rhs.den_, lhs.den_ * rhs.num_};
        }

        constexpr rational& operator+=(const rational other) noexcept { return *this = (*this + other); }
        constexpr rational& operator-=(const rational other) noexcept { return *this = (*this - other); }
        constexpr rational& operator*=(const rational other) noexcept { return *this = (*this * other); }
        constexpr rational& operator/=(const rational other) noexcept { return *this = (*this / other); }

        template <std::floating_point F>
        constexpr explicit operator F() const noexcept
        {
            return F(num_) / F(den_);
        }

        constexpr friend bool operator==(rational, rational) noexcept = default;

        constexpr friend std::strong_ordering operator<=>(const rational lhs, const rational rhs) noexcept
        {
            struct state
            {
                T num, den, quot, rem;

                constexpr state(T num, T den) noexcept: num(num), den(den), quot(num / den), rem(num % den) {}

                // Only need to call this on the first iteration, since after the first iteration
                // the numerators and denominators are all non-negative
                constexpr void fix_negative_remainder() noexcept
                {
                    if (rem < 0)
                    {
                        --quot;
                        rem += den;
                    }
                }
            } lst(lhs.num_, lhs.den_), rst(rhs.num_, rhs.den_);
            lst.fix_negative_remainder();
            rst.fix_negative_remainder();

            bool reverse = false; // The sign is reversed on every iteration

            while (true)
            {
                // Compare the whole number part
                if (lst.quot != rst.quot)
                    return reverse ? rst.quot <=> lst.quot : lst.quot <=> rst.quot;
                if (lst.rem == T(0) || rst.rem == T(0))
                    break;
                reverse = !reverse;
                lst = state{lst.den, lst.rem};
                rst = state{rst.den, rst.rem};
            }

            // Someone's remainder is 0, the non-zero one is larger if not reversed
            return reverse ? rst.rem <=> lst.rem : lst.rem <=> rst.rem;
        }

    private:
        T num_ = T(0);
        T den_ = T(1);

        constexpr void normalize() noexcept
        {
            if (num_ == T(0))
                den_ = T(1);
            else
            {
                const auto gcd = std::gcd(num_, den_) * (den_ < 0 ? -1 : 1);
                num_ /= gcd;
                den_ /= gcd;
            }
        }
    };

    template <std::signed_integral T>
    constexpr rational<T> abs(const rational<T> value) noexcept
    {
        return value.numerator() > 0 ? value : -value;
    }
}
