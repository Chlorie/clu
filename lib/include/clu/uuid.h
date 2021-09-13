#pragma once

#include <array>
#include <format>

#include "hash.h"
#include "random.h"
#include "parse.h"

namespace clu
{
    class uuid
    {
    public:
        using value_type = std::array<std::byte, 16>;

        enum class variant_type : uint8_t
        {
            ncs = 0,
            rfc4122 = 1,
            microsoft = 2,
            reserved = 3
        };

        enum class version_type : uint8_t
        {
            nil = 0,
            time_based = 1,
            dce_security = 2,
            name_based_md5 = 3,
            random = 4,
            name_based_sha1 = 5
        };
        
    private:
        value_type data_{};

        [[noreturn]] static void parse_error() { throw std::runtime_error("invalid uuid string"); }

        constexpr static bool is_hex_char(const char ch)
        {
            return (ch >= '0' && ch <= '9') ||
                (ch >= 'a' && ch <= 'f') ||
                (ch >= 'A' && ch <= 'F');
        }

    public:
        constexpr uuid() noexcept = default;
        constexpr explicit uuid(const value_type data): data_(data) {}

        constexpr explicit uuid(const u64 first, const u64 second) noexcept
        {
            constexpr auto shift_byte =
                [](const uint64_t value, const int shift) { return static_cast<std::byte>(value >> shift); };
            data_ = {
                shift_byte(first, 56), shift_byte(first, 48), shift_byte(first, 40), shift_byte(first, 32),
                shift_byte(first, 24), shift_byte(first, 16), shift_byte(first, 8), shift_byte(first, 0),
                shift_byte(second, 56), shift_byte(second, 48), shift_byte(second, 40), shift_byte(second, 32),
                shift_byte(second, 24), shift_byte(second, 16), shift_byte(second, 8), shift_byte(second, 0)
            };
        }

        [[nodiscard]] constexpr friend bool operator==(uuid, uuid) noexcept = default;
        [[nodiscard]] constexpr friend auto operator<=>(uuid, uuid) noexcept = default;

        [[nodiscard]] constexpr value_type& data() noexcept { return data_; }
        [[nodiscard]] constexpr const value_type& data() const noexcept { return data_; }

        [[nodiscard]] constexpr std::byte operator[](const size_t index) const noexcept { return data_[index]; }

        [[nodiscard]] constexpr variant_type variant() const noexcept
        {
            using enum variant_type;
            const auto n = static_cast<uint8_t>(data_[8] >> 4);
            if (n & 8u) return ncs;
            if (n & 4u) return rfc4122;
            if (n & 2u) return microsoft;
            return reserved;
        }

        [[nodiscard]] constexpr version_type version() const noexcept
        {
            return static_cast<version_type>(
                static_cast<uint8_t>(data_[6] >> 4) & 15u);
        }

        [[nodiscard]] constexpr static uuid nil() noexcept { return uuid(); }

        [[nodiscard]] static uuid from_string(std::string_view str)
        {
            if (str.starts_with('{') && str.ends_with('}'))
            {
                str.remove_prefix(1);
                str.remove_suffix(1);
            }
            if (str.size() == 32) // no dashes
            {
                const uint64_t first = parse<uint64_t>(str.substr(0, 16), 16).value();
                const uint64_t second = parse<uint64_t>(str.substr(16), 16).value();
                return uuid(first, second);
            }
            else if (str.size() == 36) // four dashes
            {
                auto first = parse_consume<uint64_t>(str, 16).value();
                if (str.size() != 28 || str[0] != '-') parse_error();
                str.remove_prefix(1);
                first = (first << 16) | parse_consume<uint64_t>(str, 16).value();
                if (str.size() != 23 || str[0] != '-') parse_error();
                str.remove_prefix(1);
                first = (first << 16) | parse_consume<uint64_t>(str, 16).value();
                if (str.size() != 18 || str[0] != '-') parse_error();
                str.remove_prefix(1);
                auto second = parse_consume<uint64_t>(str, 16).value();
                if (str.size() != 13 || str[0] != '-') parse_error();
                str.remove_prefix(1);
                second = (second << 48) | parse_consume<uint64_t>(str, 16).value();
                if (!str.empty()) parse_error();
                return uuid(first, second);
            }
            parse_error();
        }

        [[nodiscard]] static uuid generate_random() { return generate_random(random_engine()); }

        template <typename URBG>
        [[nodiscard]] static uuid generate_random(URBG&& gen)
        {
            std::uniform_int_distribution<uint64_t> dist;
            const uint64_t first = (dist(gen) & ~0xf000ull) | 0x4000ull;
            const uint64_t second = (dist(gen) & ~(3ull << 62)) | (1ull << 63);
            return uuid(first, second);
        }

        [[nodiscard]] constexpr static uuid generate_name_based_md5(
            const uuid& namespace_, const std::string_view name) noexcept
        {
            md5_hasher h;
            h.update(namespace_.data());
            detail::constexpr_update_hash_sv(h, name);
            const auto& hash = h.finalize();
            auto first = (static_cast<uint64_t>(hash[3]) << 32) | static_cast<uint64_t>(hash[2]);
            first = (first & ~0xf000ull) | 0x3000ull;
            auto second = (static_cast<uint64_t>(hash[1]) << 32) | static_cast<uint64_t>(hash[0]);
            second = (second & ~(3ull << 62)) | (1ull << 63);
            return uuid(first, second);
        }

        [[nodiscard]] constexpr static uuid generate_name_based_sha1(
            const uuid& namespace_, const std::string_view name) noexcept
        {
            sha1_hasher h;
            h.update(namespace_.data());
            detail::constexpr_update_hash_sv(h, name);
            const auto& hash = h.finalize();
            auto first = (static_cast<uint64_t>(hash[4]) << 32) | static_cast<uint64_t>(hash[3]);
            first = (first & ~0xf000ull) | 0x5000ull;
            auto second = (static_cast<uint64_t>(hash[2]) << 32) | static_cast<uint64_t>(hash[1]);
            second = (second & ~(3ull << 62)) | (1ull << 63);
            return uuid(first, second);
        }

        [[nodiscard]] std::string to_string() const
        {
            const auto u8byte = [&](const size_t i) { return static_cast<uint8_t>(data_[i]); };
            return std::format(
                "{:02x}{:02x}{:02x}{:02x}-"
                "{:02x}{:02x}-{:02x}{:02x}-"
                "{:02x}{:02x}-{:02x}{:02x}"
                "{:02x}{:02x}{:02x}{:02x}",
                u8byte(0), u8byte(1), u8byte(2), u8byte(3),
                u8byte(4), u8byte(5), u8byte(6), u8byte(7),
                u8byte(8), u8byte(9), u8byte(10), u8byte(11),
                u8byte(12), u8byte(13), u8byte(14), u8byte(15));
        }
    };

    namespace uuid_namespaces
    {
        // @formatter:off
        inline constexpr uuid dns   { 0x6ba7b8109dad11d1ull, 0x80b400c04fd430c8ull };
        inline constexpr uuid url   { 0x6ba7b8119dad11d1ull, 0x80b400c04fd430c8ull };
        inline constexpr uuid oid   { 0x6ba7b8129dad11d1ull, 0x80b400c04fd430c8ull };
        inline constexpr uuid x500dn{ 0x6ba7b8149dad11d1ull, 0x80b400c04fd430c8ull };
        // @formatter:on
    }

    inline namespace literals
    {
        inline namespace uuid_literal
        {
            [[nodiscard]] inline uuid operator""_uuid(const char* ptr, const size_t size)
            {
                return uuid::from_string({ ptr, size });
            }
        }
    }
}

template <>
struct std::hash<clu::uuid>
{
    [[nodiscard]] constexpr size_t operator()(const clu::uuid id) const noexcept
    {
        size_t hash = 0;
        for (const auto byte : id.data())
            clu::hash_combine(hash, static_cast<uint8_t>(byte));
        return hash;
    }
};

template <>
struct std::formatter<clu::uuid> : std::formatter<std::string_view>
{
    template <typename FormatContext>
    auto format(const clu::uuid& ver, FormatContext& ctx)
    {
        return std::formatter<std::string_view>::format(
            ver.to_string(), ctx);
    }
};
