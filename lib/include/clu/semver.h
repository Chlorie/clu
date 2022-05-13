#pragma once

#include <vector>
#include <string>
#include <format>
#include <algorithm>

#include "integer_literals.h"
#include "export.h"

namespace clu
{
    CLU_SUPPRESS_EXPORT_WARNING
    class CLU_API semver
    {
    public:
        constexpr semver() noexcept = default; /// Initialize the version to 0.1.0
        semver(u32 major, u32 minor, u32 patch, std::string prerelease = "", std::string build = "");

        [[nodiscard]] static semver from_string(std::string_view ver);

        [[nodiscard]] std::string to_string() const;

        [[nodiscard]] u32 major() const noexcept { return major_; }
        [[nodiscard]] u32 minor() const noexcept { return minor_; }
        [[nodiscard]] u32 patch() const noexcept { return patch_; }
        [[nodiscard]] std::string_view prerelease() const noexcept { return prerelease_; }
        [[nodiscard]] std::string_view build_metadata() const noexcept { return build_; }

        friend bool operator==(const semver&, const semver&) noexcept = default;
        CLU_API friend std::weak_ordering operator<=>(const semver& lhs, const semver& rhs);

    private:
        u32 major_ = 0;
        u32 minor_ = 1;
        u32 patch_ = 0;
        std::string prerelease_;
        std::vector<size_t> prerelease_sep_;
        std::string build_;

        // Identifier characters [0-9A-Za-z-] and the dot separator
        static constexpr bool is_valid_char(const char ch)
        {
            // clang-format off
            return
                (ch >= '0' && ch <= '9') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z') ||
                ch == '-' ||
                ch == '.';
            // clang-format on
        }

        static constexpr bool is_numeric(const std::string_view str) noexcept
        {
            return std::ranges::all_of(str, [](const char ch) { return ch >= '0' && ch <= '9'; });
        }

        [[noreturn]] static void error(const char* desc) noexcept(false);

        // Check the prerelease/build string
        // Only checks whether they contain invalid characters
        // Return: count of parts (separators + 1)
        static size_t validate_string_chars(std::string_view str);
        static void throw_on_numeric_with_leading_zero(std::string_view str);

        // Find indices of the dot separator, also validate the string
        static std::vector<size_t> validate_and_separate(std::string_view str, bool no_leading_zero = false);
        static u32 parse_u32_no_leading_zero(std::string_view& str);
        std::string_view prerelease_segment_at(size_t index) const;
        std::weak_ordering compare_prerelease(const semver& rhs) const;
    };
    CLU_RESTORE_EXPORT_WARNING

    inline namespace literals
    {
        inline namespace semver_literal
        {
            inline semver operator""_semver(const char* str, size_t length)
            {
                return semver::from_string({str, length});
            }
        } // namespace semver_literal
    } // namespace literals
} // namespace clu

template <>
struct std::formatter<clu::semver> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const clu::semver& ver, FormatContext& ctx)
    {
        return std::formatter<std::string_view>::format(ver.to_string(), ctx);
    }
};
