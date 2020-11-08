#pragma once

#include <cstdint>
#include <string_view>

namespace clu
{
    constexpr uint32_t fnv1a(const std::string_view str) noexcept
    {
        uint32_t hash = 0x811c9dc5u;
        for (const char ch : str) hash = (hash ^ static_cast<uint32_t>(ch)) * 0x01000193u;
        return hash;
    }

    namespace literals
    {
        constexpr uint32_t operator""_fnv1a(const char* str, const size_t length) noexcept { return fnv1a({ str, length }); }
    }
}
