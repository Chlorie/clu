#pragma once

#include <span>
#include <compare>

#ifdef CLU_MSVC
    #include <intrin.h>
#endif

#include "../integer_literals.h"

namespace clu
{
    template <size_t N>
    class extended_uint;

#define CLU_EXT_UINT_BINARY_OP_FWD_DECL(op)                                                                            \
    template <size_t L, size_t R>                                                                                      \
    [[nodiscard]] constexpr auto operator op(const extended_uint<L>& lhs, const extended_uint<R>& rhs) noexcept

    CLU_EXT_UINT_BINARY_OP_FWD_DECL(*);
    CLU_EXT_UINT_BINARY_OP_FWD_DECL(/);
    CLU_EXT_UINT_BINARY_OP_FWD_DECL(%);

#undef CLU_EXT_UINT_BINARY_OP_FWD_DECL

    namespace detail
    {
        struct sum_carry
        {
            u64 sum;
            bool carry;
        };

        template <bool is_add = true>
        constexpr sum_carry half_add_sub(const u64 lhs, const u64 rhs) noexcept
        {
            if constexpr (is_add)
            {
                const u64 sum = lhs + rhs;
                return {.sum = sum, .carry = sum < lhs};
            }
            else
            {
                const u64 sum = lhs - rhs;
                return {.sum = sum, .carry = sum > lhs};
            }
        }

        template <bool is_add = true>
        constexpr sum_carry full_add_sub(const u64 lhs, const u64 rhs, const bool carry) noexcept
        {
            const auto [half_sum, carry_half] = detail::half_add_sub<is_add>(lhs, rhs);
            const auto [sum, carry_full] = detail::half_add_sub<is_add>(half_sum, carry);
            return {.sum = sum, .carry = carry_half || carry_full};
        }

        struct wide_product
        {
            u64 low;
            u64 high;
        };

        constexpr wide_product long_multiply_u64(const u64 lhs, const u64 rhs) noexcept
        {
            const u64 ll = static_cast<u32>(lhs), lh = lhs >> 32;
            const u64 rl = static_cast<u32>(rhs), rh = rhs >> 32;
            const u64 low = ll * rl, high = lh * rh;
            const auto [mid, mid_carry] = half_add_sub(ll * rh, lh * rl);
            const auto [low_sum, low_carry] = half_add_sub(low, mid << 32);
            return {.low = low_sum, .high = high + (mid >> 32) + (static_cast<u64>(mid_carry) << 32) + low_carry};
        }

        constexpr wide_product wide_multiply(const u64 lhs, const u64 rhs) noexcept
        {
            CLU_IF_CONSTEVAL { return long_multiply_u64(lhs, rhs); }
            else
            {
#if defined(CLU_GCC) || defined(CLU_CLANG)
                using uint128_t = unsigned __int128;
                const uint128_t res = static_cast<uint128_t>(lhs) * static_cast<uint128_t>(rhs);
                return {.low = static_cast<u64>(res), .high = static_cast<u64>(res >> 64)};
#elif defined(CLU_MSVC)
                u64 high;
                const u64 low = _umul128(lhs, rhs, &high);
                return {.low = low, .high = high};
#else
                return long_multiply_u64(lhs, rhs);
#endif
            }
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

        template <typename... Ts>
            requires(std::convertible_to<Ts, u64> && ...) && (sizeof...(Ts) <= N)
        constexpr explicit(false) extended_uint(const Ts... values) noexcept: data_{static_cast<u64>(values)...}
        {
        }

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
    constexpr extended_uint& operator op##=(const extended_uint<M>& other) noexcept                                    \
    {                                                                                                                  \
        for (size_t i = 0; i < M; i++)                                                                                 \
            data_[i] op## = other.data_[i];                                                                            \
        return *this;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    constexpr extended_uint& operator op##=(const u64 other) noexcept                                                  \
    {                                                                                                                  \
        data_[0] op## = other;                                                                                         \
        return *this;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    [[nodiscard]] constexpr friend extended_uint operator op(extended_uint lhs, const u64 rhs) noexcept                \
    {                                                                                                                  \
        lhs.data_[0] op## = rhs;                                                                                       \
        return lhs;                                                                                                    \
    }                                                                                                                  \
                                                                                                                       \
    [[nodiscard]] constexpr friend extended_uint operator op(const u64 lhs, extended_uint rhs) noexcept                \
    {                                                                                                                  \
        rhs.data_[0] op## = lhs;                                                                                       \
        return rhs;                                                                                                    \
    }                                                                                                                  \
    static_assert(true)

        CLU_EXT_UINT_BITWISE_OP(&);
        CLU_EXT_UINT_BITWISE_OP(|);
        CLU_EXT_UINT_BITWISE_OP(^);

#undef CLU_EXT_UINT_BITWISE_OP

        // Addition/subtraction

        constexpr extended_uint& operator+=(const u64 other) noexcept
        {
            add_sub_u64_at<true>(other, 0);
            return *this;
        }

        template <size_t M>
            requires(M <= N)
        constexpr extended_uint& operator+=(const extended_uint<M>& other) noexcept
        {
            this->template add_sub_assign<true>(other);
            return *this;
        }

        constexpr extended_uint& operator++() noexcept { return *this += 1; }

        constexpr extended_uint operator++(int) noexcept
        {
            extended_uint res = *this;
            *this += 1;
            return res;
        }

        [[nodiscard]] constexpr friend extended_uint operator+(extended_uint lhs, const u64 rhs) noexcept
        {
            lhs += rhs;
            return lhs;
        }

        [[nodiscard]] constexpr friend extended_uint operator+(const u64 lhs, extended_uint rhs) noexcept
        {
            rhs += lhs;
            return rhs;
        }

        [[nodiscard]] constexpr extended_uint operator+() const noexcept { return *this; }
        [[nodiscard]] constexpr extended_uint operator-() const noexcept { return ++~*this; }

        constexpr extended_uint& operator-=(const u64 other) noexcept
        {
            add_sub_u64_at<false>(other, 0);
            return *this;
        }

        template <size_t M>
            requires(M <= N)
        constexpr extended_uint& operator-=(const extended_uint<M>& other) noexcept
        {
            this->template add_sub_assign<false>(other);
            return *this;
        }

        constexpr extended_uint& operator--() noexcept { return *this -= 1; }

        constexpr extended_uint operator--(int) noexcept
        {
            extended_uint res = *this;
            *this -= 1;
            return res;
        }

        [[nodiscard]] constexpr friend extended_uint operator-(extended_uint lhs, const u64 rhs) noexcept
        {
            lhs -= rhs;
            return lhs;
        }

        [[nodiscard]] constexpr friend extended_uint operator-(const u64 lhs, const extended_uint& rhs) noexcept
        {
            return extended_uint<1>(lhs) - rhs;
        }

        // Multiplication

        constexpr extended_uint& operator*=(const u64 other) noexcept
        {
            u64 overflow = 0;
            for (int i = 0; i < N - 1; i++)
            {
                const auto [low, high] = detail::long_multiply_u64(data_[i], other);
                const auto [low_sum, carry] = detail::half_add_sub(low, overflow);
                data_[i] = low_sum;
                overflow = high + carry;
            }
            data_[N - 1] = data_[N - 1] * other + overflow;
            return *this;
        }

        [[nodiscard]] constexpr friend extended_uint operator*(extended_uint lhs, const u64 rhs) noexcept
        {
            lhs *= rhs;
            return lhs;
        }

        [[nodiscard]] constexpr friend extended_uint operator*(const u64 lhs, extended_uint rhs) noexcept
        {
            rhs *= lhs;
            return rhs;
        }

    private:
        u64 data_[N]{};

        template <bool is_add>
        constexpr void add_sub_u64_at(u64 carry, size_t i) noexcept
        {
            using op = conditional_t<is_add, std::plus<>, std::minus<>>;
            for (; i < N - 1; i++)
            {
                if (!carry)
                    return;
                const auto [sum, next_carry] = detail::half_add_sub<is_add>(data_[i], carry);
                data_[i] = sum;
                carry = next_carry;
            }
            data_[N - 1] = op{}(data_[N - 1], carry);
        }

        template <bool is_add, size_t M>
        constexpr void add_sub_assign(const extended_uint<M>& other) noexcept
        {
            bool carry = false;
            for (size_t i = 0; i < M; i++)
            {
                const auto [sum, next_carry] = detail::full_add_sub<is_add>(data_[i], other.data_[i], carry);
                data_[i] = sum;
                carry = next_carry;
            }
            if constexpr (M < N)
                add_sub_u64_at<is_add>(carry, M);
        }

        // Friends

        template <size_t>
        friend class extended_uint;

#define CLU_EXT_UINT_BINARY_OP_FRIEND(op)                                                                              \
    template <size_t L, size_t R>                                                                                      \
    [[nodiscard]] constexpr friend auto operator op(const extended_uint<L>& lhs, const extended_uint<R>& rhs) noexcept

        CLU_EXT_UINT_BINARY_OP_FRIEND(*);
        CLU_EXT_UINT_BINARY_OP_FRIEND(/);
        CLU_EXT_UINT_BINARY_OP_FRIEND(%);

#undef CLU_EXT_UINT_BINARY_OP_FRIEND
    };

#define CLU_EXT_UINT_COMM_BINARY_OP(op)                                                                                \
    template <size_t L, size_t R>                                                                                      \
    [[nodiscard]] constexpr auto operator op(const extended_uint<L>& lhs, const extended_uint<R>& rhs) noexcept        \
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
    static_assert(true)

    CLU_EXT_UINT_COMM_BINARY_OP(&);
    CLU_EXT_UINT_COMM_BINARY_OP(|);
    CLU_EXT_UINT_COMM_BINARY_OP(^);
    CLU_EXT_UINT_COMM_BINARY_OP(+);

#undef CLU_EXT_UINT_COMM_BINARY_OP

    template <size_t L, size_t R>
    [[nodiscard]] constexpr auto operator-(const extended_uint<L>& lhs, const extended_uint<R>& rhs) noexcept
    {
        if constexpr (L < R)
            return extended_uint<R>(lhs) - rhs;
        else
        {
            auto res = lhs;
            res -= rhs;
            return res;
        }
    }

    constexpr auto test = extended_uint<2>(~u64{}, 1) * 2;
} // namespace clu
