#pragma once

#include <array>
#include <format>

#include "hash.h"
#include "random.h"
#include "parse.h"
#include "integer_literals.h"

namespace clu
{
    class alignas(8) uuid
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

        constexpr uuid() noexcept = default;
        constexpr explicit uuid(const value_type data): data_(data) {}

        constexpr explicit uuid(const u64 first, const u64 second) noexcept
        {
            constexpr auto shift_byte = [](const u64 value, const int shift)
            { return static_cast<std::byte>(value >> shift); };
            data_ = {
                shift_byte(first, 56), shift_byte(first, 48), shift_byte(first, 40), shift_byte(first, 32),
                shift_byte(first, 24), shift_byte(first, 16), shift_byte(first, 8), shift_byte(first, 0),
                shift_byte(second, 56), shift_byte(second, 48), shift_byte(second, 40), shift_byte(second, 32),
                shift_byte(second, 24), shift_byte(second, 16), shift_byte(second, 8), shift_byte(second, 0) //
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
            // clang-format off
            if (n & 8u) return ncs;
            if (n & 4u) return rfc4122;
            if (n & 2u) return microsoft;
            return reserved;
            // clang-format on
        }

        [[nodiscard]] constexpr version_type version() const noexcept
        {
            return static_cast<version_type>(static_cast<uint8_t>(data_[6] >> 4) & 15u);
        }

        [[nodiscard]] constexpr static uuid nil() noexcept { return {}; }

        template <typename URBG>
        [[nodiscard]] static uuid generate_random(URBG&& gen)
        {
            std::uniform_int_distribution<u64> dist;
            const u64 first = (dist(gen) & ~0xf000ull) | 0x4000ull;
            const u64 second = (dist(gen) & ~(3ull << 62)) | (1ull << 63);
            return uuid(first, second);
        }

        [[nodiscard]] static uuid generate_random();
        [[nodiscard]] static uuid generate_name_based_md5(const uuid& namespace_, std::string_view name) noexcept;
        [[nodiscard]] static uuid generate_name_based_sha1(const uuid& namespace_, std::string_view name) noexcept;
        [[nodiscard]] static uuid from_string(std::string_view str);

        [[nodiscard]] std::string to_string() const;

    private:
        value_type data_{};

        [[noreturn]] static void parse_error();

        constexpr static bool is_hex_char(const char ch)
        {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
        }
    };

    namespace uuid_namespaces
    {
        inline constexpr uuid dns{0x6ba7b8109dad11d1ull, 0x80b400c04fd430c8ull};
        inline constexpr uuid url{0x6ba7b8119dad11d1ull, 0x80b400c04fd430c8ull};
        inline constexpr uuid oid{0x6ba7b8129dad11d1ull, 0x80b400c04fd430c8ull};
        inline constexpr uuid x500dn{0x6ba7b8149dad11d1ull, 0x80b400c04fd430c8ull};
    } // namespace uuid_namespaces

    inline namespace literals
    {
        inline namespace uuid_literal
        {
            [[nodiscard]] inline uuid operator""_uuid(const char* ptr, const std::size_t size)
            {
                return uuid::from_string({ptr, size});
            }
        } // namespace uuid_literal
    } // namespace literals
} // namespace clu

template <>
struct std::hash<clu::uuid>
{
    [[nodiscard]] constexpr std::size_t operator()(const clu::uuid id) const noexcept
    {
        std::size_t hash = 0;
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
        return std::formatter<std::string_view>::format(ver.to_string(), ctx);
    }
};
