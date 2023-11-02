#include "clu/text/print.h"
#include "clu/text/utf_convert.h"

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

        void print_nonformatted_unicode(std::FILE* file, const std::string_view text)
        {
#ifdef _WIN32
            if (const auto fd = _fileno(file); _isatty(fd))
            {
                const std::wstring wstr = to_wstring({reinterpret_cast<const char8_t*>(text.data()), text.size()});
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
