#pragma once

#include <string_view>
#include <charconv>
#include <optional>

namespace clu
{
    template <std::integral T>
    constexpr std::optional<T> parse_consume(std::string_view& str, const int base = 10) noexcept
    {
        T value;
        const char* end = str.data() + str.size();
        const auto [ptr, ec] = std::from_chars(str.data(), end, value, base);
        if (ec != std::errc{}) return std::nullopt;
        str = { ptr, end };
        return value;
    }

    template <std::floating_point T>
    constexpr std::optional<T> parse_consume(
        std::string_view& str, const std::chars_format fmt = std::chars_format::general) noexcept
    {
        T value;
        const char* end = str.data() + str.size();
        const auto [ptr, ec] = std::from_chars(str.data(), end, value, fmt);
        if (ec != std::errc{}) return std::nullopt;
        str = { ptr, end };
        return value;
    }

    template <std::integral T>
    constexpr std::optional<T> parse(
        std::string_view str, const int base = 10) noexcept
    {
        const auto result = parse_consume<T>(str, base);
        return str.empty() ? result : std::nullopt;
    }

    template <std::floating_point T>
    constexpr std::optional<T> parse(
        std::string_view str, const std::chars_format fmt = std::chars_format::general)
    {
        const auto result = parse_consume<T>(str, fmt);
        return str.empty() ? result : std::nullopt;
    }
}
