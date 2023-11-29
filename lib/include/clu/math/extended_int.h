#pragma once

#include <span>
#include <compare>

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
        static_assert(N > 0, "An extended unsigned integer must have at least 1 component.");

        // Constructors and assignment operators

        constexpr extended_uint() noexcept = default;
        constexpr extended_uint(const extended_uint&) noexcept = default;
        constexpr explicit(false) extended_uint(const u64 value) noexcept: data_{value} {}

        constexpr explicit extended_uint(const std::span<const u64, N> span) noexcept
        {
            for (size_t i = 0; i < N; i++)
                data_[i] = span[i];
        }

        template <size_t M>
        constexpr explicit(M > N) extended_uint(const extended_uint<M>& other) noexcept
        {
            constexpr size_t less = M > N ? N : M;
            for (size_t i = 0; i < less; i++)
                data_[i] = other.data_[i];
        }

        constexpr extended_uint& operator=(const extended_uint&) noexcept = default;

        // Data accessors

        [[nodiscard]] constexpr u64* data() noexcept { return data_; }
        [[nodiscard]] constexpr const u64* data() const noexcept { return data_; }
        [[nodiscard]] constexpr std::span<u64, N> as_span() noexcept { return data_; }
        [[nodiscard]] constexpr std::span<const u64, N> as_span() const noexcept { return data_; }
        [[nodiscard]] constexpr u64& operator[](const size_t i) noexcept { return data_[i]; }
        [[nodiscard]] constexpr u64 operator[](const size_t i) const noexcept { return data_[i]; }

        // Comparison-related operators

        [[nodiscard]] constexpr bool operator==(const extended_uint&) const noexcept = default;
        [[nodiscard]] constexpr auto operator<=>(const extended_uint&) const noexcept = default;

        template <size_t M>
        [[nodiscard]] constexpr bool operator==(const extended_uint<M>& other) const noexcept
        {
            if constexpr (N < M)
                return other == *this; // Delegation
            for (size_t i = 0; i < M; i++)
                if (data_[i] != other.data_[i])
                    return false;
            for (size_t i = M; i < N; i++)
                if (data_[i] != 0)
                    return false;
            return true;
        }

        template <size_t M>
        [[nodiscard]] constexpr std::strong_ordering operator<=>(const extended_uint<M>& other) const noexcept
        {
            if constexpr (N < M)
                return 0 <=> (other <=> *this); // Delegation
            for (size_t i = M; i < N; i++)
                if (data_[i] != 0)
                    return std::strong_ordering::greater;
            for (size_t i = M; i-- > 0;)
                if (const auto cmp = data_[i] <=> other.data_[i]; cmp != 0)
                    return cmp;
            return std::strong_ordering::equal;
        }

        [[nodiscard]] constexpr bool operator==(const u64 other) const noexcept
        {
            return *this == extended_uint<1>(other);
        }

        [[nodiscard]] constexpr std::strong_ordering operator<=>(const u64 other) const noexcept
        {
            return *this <=> extended_uint<1>(other);
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            for (size_t i = 0; i < N; i++)
                if (data_[i] != 0)
                    return true;
            return false;
        }

        // Bitwise operators

        [[nodiscard]] constexpr extended_uint operator~() const noexcept
        {
            extended_uint res;
            for (size_t i = 0; i < N; i++)
                res.data_[i] = ~data_[i];
            return res;
        }

#define CLU_EXT_UINT_BITWISE_OP(op)                                                                                    \
    template <size_t M>                                                                                                \
        requires(M <= N)                                                                                               \
    [[nodiscard]] constexpr extended_uint& operator op##=(const extended_uint<M>& other) const noexcept                \
    {                                                                                                                  \
        for (size_t i = 0; i < M; i++)                                                                                 \
            data_[i] op## = other.data_[i];                                                                            \
        return *this;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    [[nodiscard]] constexpr extended_uint& operator op##=(const u64 other) const noexcept                              \
    {                                                                                                                  \
        return *this op## = extended_uint<1>(other);                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    template <size_t L, size_t R>                                                                                      \
    [[nodiscard]] constexpr friend auto operator op(const extended_uint<L>& lhs, const extended_uint<R>& rhs) noexcept \
    {                                                                                                                  \
        if constexpr (L < R)                                                                                           \
            return rhs op lhs;                                                                                         \
        else                                                                                                           \
        {                                                                                                              \
            auto res = lhs;                                                                                            \
            res op## = rhs;                                                                                            \
            return res;                                                                                                \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    [[nodiscard]] constexpr friend extended_uint operator op(const extended_uint& lhs, const u64 rhs) noexcept         \
    {                                                                                                                  \
        return lhs op extended_uint<1>(rhs);                                                                           \
    }                                                                                                                  \
                                                                                                                       \
    [[nodiscard]] constexpr friend extended_uint operator op(const u64 lhs, const extended_uint& rhs) noexcept         \
    {                                                                                                                  \
        return rhs op extended_uint<1>(lhs);                                                                           \
    }                                                                                                                  \
    static_assert(true)

        CLU_EXT_UINT_BITWISE_OP(&);
        CLU_EXT_UINT_BITWISE_OP(|);
        CLU_EXT_UINT_BITWISE_OP(^);

#undef CLU_EXT_UINT_BITWISE_OP

        // Arithmetic operators

        [[nodiscard]] constexpr extended_uint operator+() const noexcept { return *this; }

    private:
        template <typename>
        friend class extended_uint;

        u64 data_[N]{};
    };
} // namespace clu
