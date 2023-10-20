#include "clu/uuid.h"
#include "clu/parse.h"

namespace clu
{
    uuid uuid::from_string(std::string_view str)
    {
        if (str.starts_with('{') && str.ends_with('}'))
        {
            str.remove_prefix(1);
            str.remove_suffix(1);
        }
        if (str.size() == 32) // no dashes
        {
            const u64 first = parse<u64>(str.substr(0, 16), 16).value();
            const u64 second = parse<u64>(str.substr(16), 16).value();
            return uuid(first, second);
        }
        else if (str.size() == 36) // four dashes
        {
            auto first = parse_consume<u64>(str, 16).value();
            if (str.size() != 28 || str[0] != '-')
                parse_error();
            str.remove_prefix(1);
            first = (first << 16) | parse_consume<u64>(str, 16).value();
            if (str.size() != 23 || str[0] != '-')
                parse_error();
            str.remove_prefix(1);
            first = (first << 16) | parse_consume<u64>(str, 16).value();
            if (str.size() != 18 || str[0] != '-')
                parse_error();
            str.remove_prefix(1);
            auto second = parse_consume<u64>(str, 16).value();
            if (str.size() != 13 || str[0] != '-')
                parse_error();
            str.remove_prefix(1);
            second = (second << 48) | parse_consume<u64>(str, 16).value();
            if (!str.empty())
                parse_error();
            return uuid(first, second);
        }
        parse_error();
    }

    uuid uuid::generate_random() { return generate_random(thread_rng()); }

    uuid uuid::generate_name_based_md5(const uuid& namespace_, const std::string_view name) noexcept
    {
        md5_hasher h;
        h.update(namespace_.data());
        h.update(name);
        const auto& hash = h.finalize();
        auto first = (static_cast<u64>(hash[3]) << 32) | static_cast<u64>(hash[2]);
        first = (first & ~0xf000ull) | 0x3000ull;
        auto second = (static_cast<u64>(hash[1]) << 32) | static_cast<u64>(hash[0]);
        second = (second & ~(3ull << 62)) | (1ull << 63);
        return uuid(first, second);
    }

    uuid uuid::generate_name_based_sha1(const uuid& namespace_, const std::string_view name) noexcept
    {
        sha1_hasher h;
        h.update(namespace_.data());
        h.update(name);
        const auto& hash = h.finalize();
        auto first = (static_cast<u64>(hash[4]) << 32) | static_cast<u64>(hash[3]);
        first = (first & ~0xf000ull) | 0x5000ull;
        auto second = (static_cast<u64>(hash[2]) << 32) | static_cast<u64>(hash[1]);
        second = (second & ~(3ull << 62)) | (1ull << 63);
        return uuid(first, second);
    }

    std::string uuid::to_string() const
    {
        const auto u8byte = [&](const size_t i) { return static_cast<uint8_t>(data_[i]); };
        return std::format( //
            "{:02x}{:02x}{:02x}{:02x}-"
            "{:02x}{:02x}-{:02x}{:02x}-"
            "{:02x}{:02x}-{:02x}{:02x}"
            "{:02x}{:02x}{:02x}{:02x}",
            u8byte(0), u8byte(1), u8byte(2), u8byte(3), //
            u8byte(4), u8byte(5), u8byte(6), u8byte(7), //
            u8byte(8), u8byte(9), u8byte(10), u8byte(11), //
            u8byte(12), u8byte(13), u8byte(14), u8byte(15));
    }

    void uuid::parse_error() { throw std::runtime_error("invalid uuid string"); }
} // namespace clu
