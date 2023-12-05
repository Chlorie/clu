#pragma once

#include <string_view>
#include <utility>
#include <system_error>

import clu.core;

namespace clu
{
    enum class decoding_errc
    {
        ok = 0,
        need_more_input,
        invalid_sequence
    };

    std::error_code make_error_code(decoding_errc ec);

    template <typename Char, size_t N>
    struct utf_encode_result
    {
        Char code_units[N];
        u32 length;
    };
    using utf8_encode_result = utf_encode_result<char8_t, 4>;
    using utf16_encode_result = utf_encode_result<char16_t, 2>;

    constexpr std::pair<char32_t, decoding_errc> decode_one(std::u8string_view& input) noexcept
    {
        using enum decoding_errc;
        constexpr std::pair more_input_res{U'\0', need_more_input};
        constexpr std::pair invalid_seq_res{U'\0', invalid_sequence};

        constexpr u8 length_table[]{
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //
            0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 //
        };
        constexpr char8_t masks[]{u8'\0', u8'\x7f', u8'\x1f', u8'\x0f', u8'\x07'};
        constexpr u8 shifts[]{0, 18, 12, 6, 0};
        constexpr char32_t min_cp[]{U'\0', U'\0', U'\x80', U'\u0800', U'\U00010000'};

        if (input.empty())
            return more_input_res;
        const std::size_t length = length_table[input[0] >> 3];
        if (length == 0)
            return invalid_seq_res;
        if (input.size() < length)
            return more_input_res;
        bool valid = true;
        char32_t res = static_cast<u32>(input[0] & masks[length]) << 18;
        switch (length)
        {
            case 4:
                res |= input[3] & 0x3fu;
                valid &= (input[3] & 0xc0u) == 0x80u;
                [[fallthrough]];
            case 3:
                res |= (input[2] & 0x3fu) << 6;
                valid &= (input[2] & 0xc0u) == 0x80u;
                [[fallthrough]];
            case 2:
                res |= (input[1] & 0x3fu) << 12;
                valid &= (input[1] & 0xc0u) == 0x80u;
                [[fallthrough]];
            default:; // case 1
        }
        res >>= shifts[length];

        valid &= (res <= U'\U0010ffff') && (res >= min_cp[length]);
        if (!valid)
            return invalid_seq_res;
        input.remove_prefix(length);
        return {res, ok};
    }

    constexpr utf16_encode_result encode_one_utf16(char32_t cp) noexcept
    {
        if (cp <= 0xffffu)
            return {.code_units{static_cast<char16_t>(cp), u'\0'}, .length = 1};
        else
        {
            cp -= 0x10000u;
            return {
                .code_units{
                    static_cast<char16_t>((cp >> 10) + 0xd800u),
                    static_cast<char16_t>((cp & 0x3ffu) + 0xdc00u) //
                },
                .length = 2 //
            };
        }
    }

    std::wstring to_wstring(std::u8string_view text);
} // namespace clu

// clang-format off
template <> struct std::is_error_code_enum<clu::decoding_errc> : std::true_type {};
// clang-format on
