#pragma once

#include <cstdint>

namespace clu
{
    using i8 = std::int8_t;
    using u8 = std::uint8_t;
    using i16 = std::int16_t;
    using u16 = std::uint16_t;
    using i32 = std::int32_t;
    using u32 = std::uint32_t;
    using i64 = std::int64_t;
    using u64 = std::uint64_t;
}

namespace clu::inline literals::inline integer_literals
{
    [[nodiscard]] constexpr i8 operator""_i8(const unsigned long long value) noexcept { return static_cast<i8>(value); };
    [[nodiscard]] constexpr u8 operator""_u8(const unsigned long long value) noexcept { return static_cast<u8>(value); };
    [[nodiscard]] constexpr i16 operator""_i16(const unsigned long long value) noexcept { return static_cast<i16>(value); };
    [[nodiscard]] constexpr u16 operator""_u16(const unsigned long long value) noexcept { return static_cast<u16>(value); };
    [[nodiscard]] constexpr i32 operator""_i32(const unsigned long long value) noexcept { return static_cast<i32>(value); };
    [[nodiscard]] constexpr u32 operator""_u32(const unsigned long long value) noexcept { return static_cast<u32>(value); };
    [[nodiscard]] constexpr i64 operator""_i64(const unsigned long long value) noexcept { return static_cast<i64>(value); };
    [[nodiscard]] constexpr u64 operator""_u64(const unsigned long long value) noexcept { return static_cast<u64>(value); };
}
