#pragma once

#include <compare>
#include <stdexcept>
#include <array>
#include <bit>

#include "../concepts.h"

namespace clu
{
    using std::uint64_t;

    template <typename Uint>
    class wider
    {
    public:
        using narrow_type = Uint;
        template <typename> friend class wider;
        static_assert(std::is_same_v<Uint, uint64_t> || is_template_of_v<Uint, wider>,
            "The narrow Uint type should be uint64_t or wider<T>");

        static constexpr size_t narrow_bit_size = 8 * sizeof(Uint);
        static constexpr size_t bit_size = 16 * sizeof(Uint);

    private:
        Uint low_{}, high_{};

        static constexpr bool carry_add(Uint& lhs, const Uint& rhs) noexcept { return (lhs += rhs) < rhs; }

        static constexpr wider carry_multiply(const Uint& lhs, const Uint& rhs) noexcept
        {
            if constexpr (std::is_same_v<Uint, uint64_t>)
            {
                constexpr uint64_t low_bits = 0xffffffffull;
                const uint64_t lhigh = lhs >> 32, llow = lhs & low_bits;
                const uint64_t rhigh = rhs >> 32, rlow = rhs & low_bits;
                const uint64_t cross = lhigh * rlow + rhigh * llow;
                return {
                    llow * rlow + (cross << 32),
                    lhigh * rhigh + (cross >> 32)
                };
            }
            else // Uint is wider<T>
            {
                const Uint low_s = Uint::carry_multiply(lhs.low_, rhs.low_);
                const Uint cross = Uint::carry_multiply(lhs.low_, rhs.high_) + Uint::carry_multiply(lhs.high_, rhs.low_);
                const Uint high_s = Uint::carry_multiply(lhs.high_, rhs.high_);
                return {
                    { low_s.low_, low_s.high_ + cross.low_ },
                    { high_s.low_ + cross.high_, high_s.high_ }
                };
            }
        }

    public:
        constexpr wider() noexcept = default;
        constexpr wider(const Uint& low, const Uint& high) noexcept: low_(low), high_(high) {}
        constexpr explicit(false) wider(const uint64_t u64) noexcept: low_{ u64 } {}

        template <typename Uint2> requires (sizeof(Uint2) < sizeof(Uint))
        constexpr explicit(false) wider(const wider<Uint2>& other) noexcept: low_{ other } {}

        template <typename Uint2> requires (sizeof(Uint2) > sizeof(Uint))
        constexpr explicit wider(const wider<Uint2>& other) noexcept: wider(other.low_) {}

        [[nodiscard]] constexpr Uint& high() noexcept { return high_; }
        [[nodiscard]] constexpr const Uint& high() const noexcept { return high_; }
        [[nodiscard]] constexpr Uint& low() noexcept { return low_; }
        [[nodiscard]] constexpr const Uint& low() const noexcept { return low_; }

        constexpr wider& operator+=(const wider& other) noexcept
        {
            high_ += other.high_ + carry_add(low_, other.low_);
            return *this;
        }

        constexpr wider& operator-=(const wider& other) noexcept { return *this += -other; }

        [[nodiscard]] friend constexpr wider operator+(wider lhs, const wider& rhs) noexcept { return lhs += rhs; }
        [[nodiscard]] friend constexpr wider operator-(wider lhs, const wider& rhs) noexcept { return lhs += -rhs; }

        [[nodiscard]] friend constexpr wider operator*(const wider& lhs, const wider& rhs) noexcept
        {
            if (lhs == 0 || rhs == 0) return {};
            const auto [llow_, lhigh_] = carry_multiply(lhs.low_, rhs.low_);
            return { llow_, lhigh_ + lhs.low_ * rhs.high_ + lhs.high_ * rhs.low_ };
        }

        constexpr wider& operator*=(const wider& other) noexcept { return *this = *this * other; }

        [[nodiscard]] friend constexpr wider operator/(wider lhs, const wider& rhs) noexcept { return lhs.div(rhs); }
        constexpr wider& operator/=(const wider& other) noexcept { return *this = *this / other; }
        [[nodiscard]] friend constexpr wider operator%(wider lhs, const wider& rhs) noexcept { return (lhs.div(rhs), lhs); }
        constexpr wider& operator%=(const wider& other) noexcept { return (div(other), *this); }

        constexpr wider& operator&=(const wider& other) noexcept
        {
            low_ &= other.low_;
            high_ &= other.high_;
            return *this;
        }

        constexpr wider& operator|=(const wider& other) noexcept
        {
            low_ |= other.low_;
            high_ |= other.high_;
            return *this;
        }

        constexpr wider& operator^=(const wider& other) noexcept
        {
            low_ ^= other.low_;
            high_ ^= other.high_;
            return *this;
        }

        constexpr wider& operator<<=(const int shift) noexcept
        {
            if (shift >= narrow_bit_size)
            {
                high_ = low_ << (shift - narrow_bit_size);
                low_ = {};
            }
            else
            {
                high_ <<= shift;
                high_ |= (low_ >> (narrow_bit_size - shift));
                low_ <<= shift;
            }
            return *this;
        }

        constexpr wider& operator>>=(const int shift) noexcept
        {
            if (shift >= narrow_bit_size)
            {
                low_ = high_ >> (shift - narrow_bit_size);
                high_ = {};
            }
            else
            {
                low_ >>= shift;
                low_ |= (high_ << (narrow_bit_size - shift));
                high_ >>= shift;
            }
            return *this;
        }

        [[nodiscard]] friend constexpr wider operator&(wider lhs, const wider& rhs) noexcept { return lhs &= rhs; }
        [[nodiscard]] friend constexpr wider operator|(wider lhs, const wider& rhs) noexcept { return lhs |= rhs; }
        [[nodiscard]] friend constexpr wider operator^(wider lhs, const wider& rhs) noexcept { return lhs ^= rhs; }
        [[nodiscard]] friend constexpr wider operator<<(wider lhs, const int shift) noexcept { return lhs <<= shift; }
        [[nodiscard]] friend constexpr wider operator>>(wider lhs, const int shift) noexcept { return lhs >>= shift; }

        [[nodiscard]] constexpr wider operator~() const noexcept { return { ~low_, ~high_ }; }
        [[nodiscard]] constexpr wider operator+() const noexcept { return *this; }
        [[nodiscard]] constexpr wider operator-() const noexcept { return ++~*this; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return low_ || high_; }
        [[nodiscard]] friend constexpr bool operator==(const wider&, const wider&) noexcept = default;

        [[nodiscard]] friend constexpr auto operator<=>(const wider& lhs, const wider& rhs) noexcept
        {
            return lhs.high_ == rhs.high_
                       ? lhs.low_ <=> rhs.low_
                       : lhs.high_ <=> rhs.high_;
        }

        constexpr wider& operator++() noexcept
        {
            if (!++low_) ++high_;
            return *this;
        }

        constexpr wider& operator--() noexcept
        {
            if (!~--low_) --high_;
            return *this;
        }

        constexpr wider operator++(int) noexcept
        {
            const wider result = *this;
            operator++();
            return result;
        }

        constexpr wider operator--(int) noexcept
        {
            const wider result = *this;
            operator--();
            return result;
        }

        constexpr wider div(wider<Uint> other) noexcept;
    };

    // Count leading zeros

    [[nodiscard]] constexpr int countl_zero(const uint64_t value) noexcept { return std::countl_zero(value); }

    template <typename Uint>
    [[nodiscard]] constexpr int countl_zero(const wider<Uint>& value) noexcept
    {
        constexpr int uint_bits = 8 * sizeof(Uint);
        const int high = countl_zero(value.high());
        return high == uint_bits ? uint_bits + countl_zero(value.low()) : high;
    }

    // Division related

    template <typename Uint>
    constexpr wider<Uint> wider<Uint>::div(wider<Uint> other) noexcept
    {
        const int shift = countl_zero(other) - countl_zero(*this);
        if (shift < 0) return {};
        wider quotient;
        wider bit = wider(1) << shift;
        other <<= shift;
        while (bit)
        {
            if (*this >= other)
            {
                quotient |= bit;
                *this -= other;
            }
            other >>= 1;
            bit >>= 1;
        }
        return quotient;
    }

    template <typename Uint>
    struct wdiv_t
    {
        wider<Uint> quot;
        wider<Uint> rem;
    };

    template <typename Uint>
    [[nodiscard]] constexpr wdiv_t<Uint> div(wider<Uint> lhs, const wider<Uint>& rhs) noexcept
    {
        return { lhs.div(rhs), lhs };
    }

    // Type aliases

    using uint128_t = wider<uint64_t>;
    using uint256_t = wider<uint128_t>;
    using uint512_t = wider<uint256_t>;
    using uint1024_t = wider<uint512_t>;
    using uint2048_t = wider<uint1024_t>;

    // User-defined literals

    class parsing_error final : std::runtime_error
    {
    public:
        using runtime_error::runtime_error;
    };

    namespace detail
    {
        constexpr void ensure_not_floating_point(const char* ptr)
        {
            bool is_hex = false;
            for (const char* p = ptr; *p != 0; ++p)
            {
                if (*p == 'x' || *p == 'X')
                {
                    is_hex = true;
                    continue;
                }
                if (*p == 'p' || *p == 'P' || *p == '.' ||
                    ((*p == 'e' || *p == 'E') && !is_hex))
                    throw parsing_error("cannot use floating point numbers as wider integer literals");
            }
        }

        constexpr uint64_t get_base(const char*& ptr)
        {
            if (*ptr != '0' || *++ptr == 0) return 10;
            switch (*ptr)
            {
                case 'x':
                case 'X': // 0x...
                    ++ptr;
                    return 16;
                case 'b':
                case 'B': // 0b...
                    ++ptr;
                    return 2;
                default: // 0...
                    return 8;
            }
        }

        inline constexpr auto char_index = []
        {
            std::array<int, 103> result{}; // 102 is 'f'
            for (int i = 0; i < 10; i++) result[i + '0'] = i;
            for (int i = 0; i < 6; i++) result[i + 'A'] = result[i + 'a'] = i + 10;
            return result;
        }();

        template <typename Wider>
        constexpr const uint64_t& get_highest_u64(const Wider& value)
        {
            if constexpr (std::is_same_v<uint64_t, typename Wider::narrow_type>)
                return value.high();
            else
                return get_highest_u64(value.high());
        }

        template <typename Wider>
        constexpr Wider parse_udl(const char* ptr)
        {
            ensure_not_floating_point(ptr);
            constexpr auto overflow = [] { throw std::overflow_error("the integer literal is too large"); };
            const uint64_t base = get_base(ptr);
            const uint64_t overflow_thres = ~0ull / base;
            Wider result;
            const uint64_t& high_est = get_highest_u64(result);
            for (; *ptr != 0; ++ptr)
            {
                if (*ptr == '\'') continue;
                if (high_est > overflow_thres) overflow();
                const Wider new_result = result * base + char_index[*ptr];
                if (new_result < result) overflow();
                result = new_result;
            }
            return result;
        }
    }
}

namespace clu::inline literals::inline wide_integer_literals
{
    // TODO: change to consteval once MSVC fixes its issues
    
    constexpr uint128_t operator""_u128(const char* str) { return detail::parse_udl<uint128_t>(str); }
    constexpr uint256_t operator""_u256(const char* str) { return detail::parse_udl<uint256_t>(str); }
    constexpr uint512_t operator""_u512(const char* str) { return detail::parse_udl<uint512_t>(str); }
    constexpr uint1024_t operator""_u1024(const char* str) { return detail::parse_udl<uint1024_t>(str); }
    constexpr uint2048_t operator""_u2048(const char* str) { return detail::parse_udl<uint2048_t>(str); }
}
