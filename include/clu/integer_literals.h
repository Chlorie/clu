#pragma once

#include <cstdint>

namespace clu
{
    namespace detail
    {
        struct narrower
        {
            unsigned long long value;

            template <typename T>
            explicit(false) operator T() const noexcept { return static_cast<T>(value); }
        };
    }

    inline namespace literals
    {
        inline namespace integer_literals
        {
            [[nodiscard]] constexpr int8_t operator""_i8(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr uint8_t operator""_u8(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr int16_t operator""_i16(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr uint16_t operator""_u16(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr int32_t operator""_i32(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr uint32_t operator""_u32(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr int64_t operator""_i64(const unsigned long long value) noexcept { return detail::narrower(value); }
            [[nodiscard]] constexpr uint64_t operator""_u64(const unsigned long long value) noexcept { return detail::narrower(value); }
        }
    }
}
