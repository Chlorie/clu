#pragma once

#include <string>
#include <ranges>
#include <algorithm>

#include "concepts.h"
#include "type_traits.h"

namespace clu
{
    template <typename T>
    concept char_type = same_as_any_of<T, char, unsigned char, signed char, char8_t, char16_t, char32_t>;

    template <typename Range> requires
        std::ranges::contiguous_range<Range> &&
        std::ranges::sized_range<Range> &&
        char_type<std::ranges::range_value_t<Range>>
    [[nodiscard]] constexpr auto to_string_view(const Range& range) noexcept
    -> std::basic_string_view<std::ranges::range_value_t<Range>>
    {
        constexpr auto diff = static_cast<std::size_t>(std::is_array_v<Range>); // null terminator
        return { std::ranges::data(range), std::ranges::size(range) - diff };
    }

    template <char_type Char>
    [[nodiscard]] constexpr std::basic_string_view<Char> to_string_view(const Char* ptr) noexcept { return ptr; }

    namespace detail
    {
        template <typename T>
        concept sv_like = requires(const T& value) { clu::to_string_view(value); };

        template <typename T>
        using char_type_of_t = typename decltype(clu::to_string_view(std::declval<T>()))::value_type;
    }

    template <detail::sv_like T>
    [[nodiscard]] constexpr std::size_t strlen(const T& str) noexcept { return clu::to_string_view(str).length(); }

    template <
        detail::sv_like SrcType, detail::sv_like FromType, detail::sv_like ToType,
        std::output_iterator<detail::char_type_of_t<SrcType>> OutIt> requires
        clu::all_same_v<detail::char_type_of_t<SrcType>, detail::char_type_of_t<FromType>, detail::char_type_of_t<ToType>>
    constexpr OutIt replace_all_copy(OutIt output, const SrcType& source, const FromType& from, const ToType& to)
    {
        auto src_sv = clu::to_string_view(source);
        const auto from_sv = clu::to_string_view(from);
        const auto to_sv = clu::to_string_view(to);
        while (true)
        {
            if (const std::size_t pos = src_sv.find(from_sv);
                pos != std::string_view::npos)
            {
                output = std::ranges::copy_n(src_sv.data(), static_cast<std::ptrdiff_t>(pos), output).out;
                output = std::ranges::copy(to_sv, output).out;
                src_sv.remove_prefix(pos + from_sv.length());
            }
            else
                return std::ranges::copy(src_sv, output).out;
        }
    }

    template <
        char_type Char, typename Traits, typename Alloc,
        detail::sv_like FromType, detail::sv_like ToType>
        requires clu::all_same_v<Char, detail::char_type_of_t<FromType>, detail::char_type_of_t<ToType>>
    constexpr void replace_all(std::basic_string<Char, Traits, Alloc>& source, const FromType& from, const ToType& to)
    {
        using string = std::basic_string<Char, Traits, Alloc>;
        const auto from_sv = clu::to_string_view(from);
        const auto to_sv = clu::to_string_view(to);
        if (to_sv.length() > from_sv.length())
        {
            string result;
            clu::replace_all_copy(std::back_inserter(result), source, from, to);
            source = std::move(result);
            return;
        }
        if (to_sv.length() == from_sv.length())
        {
            std::size_t pos = 0;
            while (true)
            {
                pos = source.find(from_sv, pos);
                if (pos == string::npos) return;
                std::ranges::copy(to_sv, &source[pos]);
            }
        }
        std::size_t pos = 0, write_pos = 0;
        while (true)
        {
            if (const std::size_t new_pos = source.find(from_sv, pos);
                new_pos != string::npos)
            {
                const auto diff = new_pos - pos;
                std::ranges::copy_n(&source[pos], static_cast<std::ptrdiff_t>(diff), &source[write_pos]);
                write_pos += diff;
                std::ranges::copy(to_sv, &source[write_pos + diff]);
                write_pos += to_sv.length();
                pos = new_pos + from_sv.length();
            }
            else
            {
                const auto remain = source.length() - pos;
                std::ranges::copy_n(&source[pos], static_cast<std::ptrdiff_t>(remain), &source[write_pos]);
                source.resize(write_pos + remain);
                return;
            }
        }
    }
}
