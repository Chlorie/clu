#pragma once

#include <span>

#include "../integer_literals.h"

namespace clu
{
    namespace detail
    {
        struct sum_carry
        {
            u64 sum;
            bool carry;
        };

        constexpr sum_carry half_adder(const u64 lhs, const u64 rhs) noexcept
        {
            const u64 sum = lhs + rhs;
            return {.sum = sum, .carry = sum < lhs};
        }

        constexpr sum_carry full_adder(const u64 lhs, const u64 rhs, const bool carry) noexcept
        {
            const auto [half_sum, carry_half] = half_adder(lhs, rhs);
            const auto [sum, carry_full] = half_adder(half_sum, carry);
            return {.sum = sum, .carry = carry_half || carry_full};
        }
    } // namespace detail

    template <size_t N>
    class extended_uint
    {
    public:
        constexpr extended_uint() noexcept = default;
        constexpr explicit(false) extended_uint(const u64 value) noexcept: data_{value} {}

        constexpr explicit extended_uint(const std::span<const u64, N> span) noexcept
        {
            for (size_t i = 0; i < N; i++)
                data_[i] = span[i];
        }

        [[nodiscard]] constexpr u64* data() noexcept { return data_; }
        [[nodiscard]] constexpr const u64* data() const noexcept { return data_; }
        [[nodiscard]] constexpr std::span<u64, N> as_span() noexcept { return data_; }
        [[nodiscard]] constexpr std::span<const u64, N> as_span() const noexcept { return data_; }

        [[nodiscard]] constexpr extended_uint operator+() const noexcept { return *this; }

        [[nodiscard]] constexpr extended_uint operator~() const noexcept
        {
            extended_uint res;
            for (size_t i = 0; i < N; i++)
                res.data_[i] = ~data_[i];
            return res;
        }

    private:
        u64 data_[N]{};
    };
} // namespace clu
