#pragma once

#include <vector>
#include <string>
#include <format>

#include "parse.h"
#include "integer_literals.h"

namespace clu
{
    class semver
    {
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
            return (ch >= '0' && ch <= '9') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z') ||
                ch == '-' || ch == '.';
        }

        static constexpr bool is_numeric(const std::string_view str) noexcept
        {
            return std::ranges::all_of(str,
                [](const char ch) { return ch >= '0' && ch <= '9'; });
        }

        [[noreturn]] static void error(const char* desc) noexcept(false) { throw std::runtime_error(desc); }

        // Check the prerelease/build string
        // Only checks whether they contain invalid characters
        // Return: count of parts (separators + 1)
        static size_t validate_string_chars(const std::string_view str)
        {
            if (str.empty()) return 0;
            if (!std::ranges::all_of(str, is_valid_char))
                throw std::runtime_error("invalid semver string");
            return std::ranges::count(str, '.') + 1;
        }

        static void throw_on_numeric_with_leading_zero(const std::string_view str)
        {
            if (str.size() > 1 && is_numeric(str) && str[0] == '0')
                throw std::runtime_error("semver numeric identifier cannot start with a leading zero");
        }

        // Find indices of the dot separator, also validate the string
        static std::vector<size_t> validate_and_separate(
            const std::string_view str, const bool no_leading_zero = false)
        {
            static constexpr auto npos = std::string_view::npos;
            if (str.empty()) return {};
            std::vector<size_t> result;
            result.reserve(validate_string_chars(str));
            size_t offset = 0;
            while (true)
            {
                offset = str.find('.', offset);
                result.push_back(offset == npos ? str.size() : offset);

                const size_t begin = result.size() == 1 ? 0 : result[result.size() - 2] + 1;
                const size_t length = result.back() - begin;
                const std::string_view sub = str.substr(begin, length);
                if (sub.empty()) error("empty semver identifier");
                if (no_leading_zero) throw_on_numeric_with_leading_zero(sub);

                if (offset == npos) break;
                offset++;
            }
            return result;
        }

        static u32 parse_u32_no_leading_zero(std::string_view& str)
        {
            const std::string_view original = str;
            const auto res = parse_consume<u32>(str);
            if (!res) error("failed to parse integer for semver");
            if (str.data() - original.data() > 1 && original[0] == '0') error("semver integer part cannot have leading zeros");
            return *res;
        }

        std::string_view prerelease_segment_at(const size_t index) const
        {
            const std::string_view view = prerelease_;
            if (index == 0) return view.substr(0, prerelease_sep_[0]);
            const size_t begin = prerelease_sep_[index - 1] + 1;
            const size_t length = prerelease_sep_[index] - begin;
            return view.substr(begin, length);
        }

        std::weak_ordering compare_prerelease(const semver& rhs) const
        {
            // this or rhs does not have prerelease ver
            if (prerelease_.empty())
                return rhs.prerelease_.empty() ? std::weak_ordering::equivalent : std::weak_ordering::greater;
            else if (rhs.prerelease_.empty())
                return std::weak_ordering::less;

            const auto shorter = std::min(prerelease_sep_.size(), rhs.prerelease_sep_.size());
            // compare each segment pair
            for (size_t i = 0; i < shorter; i++)
            {
                const auto lseg = prerelease_segment_at(i);
                const auto rseg = rhs.prerelease_segment_at(i);
                if (is_numeric(lseg) && is_numeric(rseg)) // both are numeric, compare as integers
                {
                    const auto lnum = parse<u32>(lseg).value();
                    const auto rnum = parse<u32>(rseg).value();
                    if (const auto comp = lnum <=> rnum; comp != 0)
                        return comp;
                }
                else
                {
                    if (const auto comp = lseg <=> rseg; comp != 0)
                        return comp;
                }
            }

            // longer one is greater
            return prerelease_sep_.size() <=> rhs.prerelease_sep_.size();
        }

    public:
        constexpr semver() noexcept = default; /// Initialize the version to 0.1.0

        semver(const u32 major, const u32 minor, const u32 patch,
            std::string prerelease = "", std::string build = ""):
            major_(major), minor_(minor), patch_(patch),
            prerelease_(std::move(prerelease)), prerelease_sep_(validate_and_separate(prerelease_, true)),
            build_(std::move(build)) { (void)validate_and_separate(build_); }

        [[nodiscard]] static semver from_string(std::string_view ver)
        {
            if (ver.empty()) throw std::runtime_error("empty string is not a valid semver");
            if (ver[0] == 'v') ver.remove_prefix(1);

            semver result;
            result.major_ = parse_u32_no_leading_zero(ver);

            if (!ver.starts_with('.')) error("a dot must follow the major version");
            ver.remove_prefix(1);

            result.minor_ = parse_u32_no_leading_zero(ver);
            if (!ver.starts_with('.')) error("a dot must follow the minor version");
            ver.remove_prefix(1);

            result.patch_ = parse_u32_no_leading_zero(ver);
            if (!ver.empty())
            {
                if (ver[0] == '-') // prerelease
                {
                    ver.remove_prefix(1);
                    const auto iter = std::ranges::find_if_not(ver, is_valid_char);
                    if (iter != ver.end() && *iter != '+') error("invalid character following prerelease version");
                    if (iter == ver.begin()) error("empty prerelease version");
                    const auto consume_size = static_cast<size_t>(iter - ver.begin());
                    const std::string_view prerelease_view = ver.substr(0, consume_size);
                    result.prerelease_sep_ = validate_and_separate(prerelease_view, true);
                    result.prerelease_ = prerelease_view;
                    ver.remove_prefix(consume_size);
                }
                if (!ver.empty()) // build metadata
                {
                    if (ver[0] != '+') error("invalid character following patch version");
                    ver.remove_prefix(1);
                    if (ver.empty()) error("empty build version");
                    (void)validate_and_separate(ver);
                    result.build_ = ver;
                }
            }

            return result;
        }

        [[nodiscard]] std::string to_string() const
        {
            return std::format("{}.{}.{}{}{}{}{}",
                major_, minor_, patch_,
                prerelease_.empty() ? "" : "-", prerelease_,
                build_.empty() ? "" : "+", build_);
        }

        [[nodiscard]] u32 major() const noexcept { return major_; }
        [[nodiscard]] u32 minor() const noexcept { return minor_; }
        [[nodiscard]] u32 patch() const noexcept { return patch_; }
        [[nodiscard]] std::string_view prerelease() const noexcept { return prerelease_; }
        [[nodiscard]] std::string_view build_metadata() const noexcept { return build_; }

        [[nodiscard]] friend bool operator==(const semver&, const semver&) noexcept = default;

        [[nodiscard]] friend std::weak_ordering operator<=>(const semver& lhs, const semver& rhs)
        {
            if (lhs.major_ != rhs.major_) return lhs.major_ <=> rhs.major_;
            if (lhs.minor_ != rhs.minor_) return lhs.minor_ <=> rhs.minor_;
            if (lhs.patch_ != rhs.patch_) return lhs.patch_ <=> rhs.patch_;
            return lhs.compare_prerelease(rhs);
        }
    };

    inline namespace literals
    {
        inline namespace semver_literal
        {
            inline semver operator""_semver(const char* str, size_t length) { return semver::from_string({ str, length }); }
        }
    }
}

template <>
struct std::formatter<clu::semver> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const clu::semver& ver, FormatContext& ctx)
    {
        return std::formatter<std::string_view>::format(
            ver.to_string(), ctx);
    }
};
