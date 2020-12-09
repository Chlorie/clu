#pragma once

#include <cstdint>
#include <bit>
#include <functional>
#include <string_view>

namespace clu
{
    inline constexpr uint32_t fnv_prime_32 = 0x01000193u;
    inline constexpr uint64_t fnv_prime_64 = 0x00000100000001b3ull;

    inline constexpr uint32_t fnv_offset_basis_32 = 0x811c9dc5u;
    inline constexpr uint64_t fnv_offset_basis_64 = 0xcbf29ce484222325ull;

    [[nodiscard]] constexpr uint32_t fnv1a32(const std::string_view bytes, uint32_t hash = fnv_offset_basis_32) noexcept
    {
        for (const char byte : bytes)
            hash = (hash ^ static_cast<uint32_t>(byte)) * fnv_prime_32;
        return hash;
    }

    [[nodiscard]] constexpr uint64_t fnv1a64(const std::string_view bytes, uint64_t hash = fnv_offset_basis_64) noexcept
    {
        for (const char byte : bytes)
            hash = (hash ^ static_cast<uint64_t>(byte)) * fnv_prime_64;
        return hash;
    }

    [[nodiscard]] constexpr size_t fnv1a(const std::string_view bytes) noexcept
    {
        if constexpr (sizeof(size_t) == 4)
            return fnv1a32(bytes);
        else if constexpr (sizeof(size_t) == 8)
            return fnv1a64(bytes);
    }

    [[nodiscard]] constexpr size_t fnv1a(const std::string_view bytes, size_t hash) noexcept
    {
        if constexpr (sizeof(size_t) == 4)
            return fnv1a32(bytes, hash);
        else if constexpr (sizeof(size_t) == 8)
            return fnv1a64(bytes, hash);
    }

    namespace literals
    {
        [[nodiscard]] constexpr uint32_t operator""_fnv1a32(const char* bytes, const size_t length) noexcept
        {
            return fnv1a32({ bytes, length });
        }

        [[nodiscard]] constexpr uint64_t operator""_fnv1a64(const char* bytes, const size_t length) noexcept
        {
            return fnv1a64({ bytes, length });
        }

        [[nodiscard]] constexpr size_t operator""_fnv1a(const char* bytes, const size_t length) noexcept
        {
            return fnv1a({ bytes, length });
        }
    }

    inline constexpr uint32_t golden_ratio_32 = 0x9e3779b9u;
    inline constexpr uint64_t golden_ratio_64 = 0x9e3779b97f4a7c15ull;

    template <typename T>
    void hash_combine(size_t& seed, const T& value)
    {
        static constexpr size_t golden_ratio = []
        {
            if constexpr (sizeof(size_t) == 4)
                return golden_ratio_32;
            else if constexpr (sizeof(size_t) == 8)
                return golden_ratio_64;
        }();
        seed ^= std::hash<T>{}(value) + golden_ratio +
            std::rotl(seed, 6) + std::rotr(seed, 2);
    }
}
