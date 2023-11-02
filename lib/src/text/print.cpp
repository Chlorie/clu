#include "clu/text/print.h"

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#endif

namespace clu
{
    namespace
    {
        consteval bool ordinary_literal_encoding_is_utf8()
        {
            constexpr std::string_view hello_codepoints = "\u4f60\u597d";
            constexpr std::string_view hello_bytes = "\xe4\xbd\xa0\xe5\xa5\xbd";
            return hello_codepoints == hello_bytes;
        }

        [[noreturn]] void throw_system_error_from_errno()
        {
            throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)));
        }

        void print_nonformatted_nonunicode(std::FILE* file, const std::string_view text)
        {
            if (std::fwrite(text.data(), 1, text.size(), file) != text.size()) [[unlikely]]
                throw_system_error_from_errno();
        }

#ifdef _WIN32
        std::wstring utf8_to_wstring(const std::string_view utf8)
        {
            // TODO: size() might be greater than max int
            const int length = MultiByteToWideChar(
                CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0); // -1 for null terminator
            std::wstring res(length, L'\0');
            if (!MultiByteToWideChar(
                    CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), res.data(), static_cast<int>(res.size())))
                throw std::format_error("failed to convert utf-8 into wide string");
            return res;
        }
#endif

        void print_nonformatted_unicode(std::FILE* file, const std::string_view text)
        {
#ifdef _WIN32
            if (const auto fd = _fileno(file); _isatty(fd))
            {
                const std::wstring wstr = utf8_to_wstring(text);
                if (DWORD written = 0;
                    !WriteConsoleW(reinterpret_cast<void*>(_get_osfhandle(fd)), // NOLINT(performance-no-int-to-ptr)
                        wstr.c_str(), static_cast<DWORD>(wstr.size()), &written, nullptr))
                    throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
                return;
            }
#endif

            if (std::fwrite(text.data(), 1, text.size(), file) != text.size()) [[unlikely]]
                throw_system_error_from_errno();
        }
    } // namespace

    void print_nonformatted(std::FILE* file, const std::string_view text)
    {
        if constexpr (ordinary_literal_encoding_is_utf8())
            print_nonformatted_unicode(file, text);
        else
            print_nonformatted_nonunicode(file, text);
    }
} // namespace clu
