#include "clu/text/utf_convert.h"

namespace clu
{
    namespace
    {
        struct decoding_error_category final : std::error_category
        {
            const char* name() const noexcept override { return "Decoding Error"; }

            std::string message(const int value) const override
            {
                using enum decoding_errc;
                switch (static_cast<decoding_errc>(value))
                {
                    case ok: return "ok";
                    case need_more_input: return "need more input";
                    case invalid_sequence: return "invalid code unit sequence";
                    default: return "unknown error";
                }
            }
        } const decoding_error_category{};
    } // namespace

    std::error_code make_error_code(const decoding_errc ec) { return {static_cast<int>(ec), decoding_error_category}; }

    std::wstring to_wstring(std::u8string_view text)
    {
        std::wstring res;
        res.reserve(text.size());
        while (!text.empty())
        {
            const auto [cp, errc] = decode_one(text);
            if (errc != decoding_errc::ok)
                throw std::system_error(errc);
            static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "Unknown wchar_t type");
            if constexpr (sizeof(wchar_t) == 4)
                res.push_back(static_cast<wchar_t>(cp));
            else // On Windows wchar_t is UTF-16
            {
                const auto [arr, len] = encode_one_utf16(cp);
                res.push_back(static_cast<wchar_t>(arr[0]));
                if (len == 2)
                    res.push_back(static_cast<wchar_t>(arr[1]));
            }
        }
        return res;
    }
} // namespace clu
