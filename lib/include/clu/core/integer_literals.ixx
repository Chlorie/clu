module;
#include <cstdint>
#include <cstddef>
#include <type_traits>

export module clu.core:integer_literals;

export namespace clu
{
    using i8 = std::int8_t; ///< 8-bit signed integer type.
    using u8 = std::uint8_t; ///< 8-bit unsigned integer type.
    using i16 = std::int16_t; ///< 16-bit signed integer type.
    using u16 = std::uint16_t; ///< 16-bit unsigned integer type.
    using i32 = std::int32_t; ///< 32-bit signed integer type.
    using u32 = std::uint32_t; ///< 32-bit unsigned integer type.
    using i64 = std::int64_t; ///< 64-bit signed integer type.
    using u64 = std::uint64_t; ///< 64-bit unsigned integer type.
    using size_t = std::size_t; ///< Unsigned size type.
    using ssize_t = std::make_signed_t<std::size_t>; ///< Signed integer type corresponding to `size_t`.
} // namespace clu

namespace clu::inline literals::inline integer_literals
{
#define CLU_INT_LIT(T, lit)                                                                                            \
    export [[nodiscard]] constexpr T operator""_##lit(const unsigned long long value) noexcept                         \
    {                                                                                                                  \
        return static_cast<T>(value);                                                                                  \
    }                                                                                                                  \
    static_assert(true)

    CLU_INT_LIT(i8, i8);
    CLU_INT_LIT(u8, u8);
    CLU_INT_LIT(i16, i16);
    CLU_INT_LIT(u16, u16);
    CLU_INT_LIT(i32, i32);
    CLU_INT_LIT(u32, u32);
    CLU_INT_LIT(i64, i64);
    CLU_INT_LIT(u64, u64);
    CLU_INT_LIT(size_t, uz);
    CLU_INT_LIT(ssize_t, z);
    CLU_INT_LIT(std::byte, b);
} // namespace clu::inline literals::inline integer_literals
