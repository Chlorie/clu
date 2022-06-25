#pragma once

#include <cstdint>
#include <bit>
#include <functional>
#include <array>
#include <string_view>

#include "buffer.h"

namespace clu
{
    template <typename T>
    concept hasher = requires(T& h, const const_buffer bytes)
    {
        h.update(bytes);
        h.finalize();
    };

    template <hasher H>
    using digest_type = std::remove_cvref_t<decltype(std::declval<H&>().finalize())>;

    class fnv1a32_hasher final
    {
    private:
        static constexpr uint32_t fnv_prime = 0x01000193u;
        uint32_t hash_ = 0x811c9dc5u; // 32-bit FNV offset basis

    public:
        constexpr void update(const const_buffer bytes) noexcept
        {
            for (const auto byte : bytes.as_span())
                hash_ = (hash_ ^ static_cast<uint32_t>(byte)) * fnv_prime;
        }
        [[nodiscard]] constexpr uint32_t finalize() const noexcept { return hash_; }
    };

    class fnv1a64_hasher final
    {
    private:
        static constexpr uint64_t fnv_prime = 0x00000100000001b3ull;
        uint64_t hash_ = 0xcbf29ce484222325ull; // 64-bit FNV offset basis

    public:
        constexpr void update(const const_buffer bytes) noexcept
        {
            for (const auto byte : bytes.as_span())
                hash_ = (hash_ ^ static_cast<uint64_t>(byte)) * fnv_prime;
        }
        [[nodiscard]] constexpr uint64_t finalize() const noexcept { return hash_; }
    };

    using fnv1a_hasher = std::conditional_t<sizeof(size_t) == 4, fnv1a32_hasher, fnv1a64_hasher>;

    namespace detail
    {
        // Convert 4 bytes into 1 uint32_t
        template <std::endian E>
        constexpr uint32_t to_u32(const const_buffer buffer) noexcept
        {
            // clang-format off
            if constexpr (E == std::endian::big)
                return
                    (static_cast<uint32_t>(buffer[0]) << 24) |
                    (static_cast<uint32_t>(buffer[1]) << 16) |
                    (static_cast<uint32_t>(buffer[2]) << 8)  |
                    static_cast<uint32_t>(buffer[3]);
            else
                return
                    static_cast<uint32_t>(buffer[0])         |
                    (static_cast<uint32_t>(buffer[1]) << 8)  |
                    (static_cast<uint32_t>(buffer[2]) << 16) |
                    (static_cast<uint32_t>(buffer[3]) << 24);
            // clang-format on
        }

        // Convert `byte_size` bytes into uint32_t array
        template <size_t byte_size, std::endian E>
            requires(byte_size % 4 == 0)
        constexpr auto to_u32_array(const_buffer buffer) noexcept
        {
            CLU_ASSERT(buffer.size() >= byte_size, "buffer size not enough");
            std::array<uint32_t, byte_size / 4> result{};
            for (uint32_t& v : result)
            {
                v = to_u32<E>(buffer);
                buffer += 4;
            }
            return result;
        }

        constexpr uint32_t switch_byte_order_u32(const uint32_t value) noexcept
        {
            // clang-format off
            const std::byte bytes[] = {
                static_cast<std::byte>(value >> 24),
                static_cast<std::byte>(value >> 16),
                static_cast<std::byte>(value >> 8),
                static_cast<std::byte>(value)
            };
            // clang-format on
            return to_u32<std::endian::little>(bytes);
        }

        template <typename H, std::endian E>
        class hasher_crtp_impl
        {
        protected:
            static constexpr size_t block_size = 64;
            static constexpr size_t block32_size = block_size / sizeof(uint32_t);
            using block32_t = std::array<uint32_t, block32_size>;

            uint64_t length_ = 0;
            std::array<std::byte, block_size> block_{};

            constexpr H& self() noexcept { return *static_cast<H*>(this); }

        public:
            constexpr void update(const_buffer bytes) noexcept
            {
                if (bytes.empty())
                    return;
                length_ += bytes.copy_to_consume(mutable_buffer(block_) += static_cast<size_t>(length_ % block_size));
                while (length_ % block_size == 0)
                {
                    block32_t block = detail::to_u32_array<block_size, E>(block_);
                    self().process_block(block);
                    if (bytes.empty())
                        return;
                    length_ += bytes.copy_to_consume(block_);
                }
            }

            [[nodiscard]] constexpr const auto& finalize() noexcept
            {
                constexpr auto high_bit = static_cast<std::byte>(0x80);
                constexpr size_t offset = block_size - sizeof(uint64_t); // 56
                const size_t leftover(length_ % block_size);

                // Fill in the 0x80 00 00 00... thing
                block_[leftover] = high_bit;
                if (leftover + 1 > offset)
                {
                    for (size_t i = leftover + 1; i < block_size; i++)
                        block_[i] = std::byte{};
                    block32_t block = detail::to_u32_array<block_size, E>(block_);
                    self().process_block(block);
                    for (size_t i = 0; i < offset; i++)
                        block_[i] = std::byte{};
                }
                else if (leftover + 1 < offset)
                {
                    for (size_t i = leftover + 1; i < offset; i++)
                        block_[i] = std::byte{};
                }

                // Copy the length and process the last chunk
                const uint64_t length64 = 8 * length_;
                block32_t last_block = detail::to_u32_array<block_size, E>(block_);
                if constexpr (E == std::endian::big)
                {
                    last_block[block32_size - 2] = static_cast<uint32_t>(length64 >> 32);
                    last_block[block32_size - 1] = static_cast<uint32_t>(length64);
                }
                else
                {
                    last_block[block32_size - 2] = static_cast<uint32_t>(length64);
                    last_block[block32_size - 1] = static_cast<uint32_t>(length64 >> 32);
                }
                self().process_block(last_block);

                return self().get_digest();
            }
        };
    } // namespace detail

    class sha1_hasher final : detail::hasher_crtp_impl<sha1_hasher, std::endian::big>
    {
    private:
        friend hasher_crtp_impl;

        std::array<uint32_t, 5> digest_{0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};

        // Adapted from https://github.com/vog/sha1/blob/master/sha1.hpp
        constexpr void process_block(block32_t& block) noexcept
        {
            // Extended part of the block (16~79)
            const auto extended = [&](const size_t i) {
                return block[i] =
                           std::rotl(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i], 1);
            };

            // SHA1 operations
            const auto r0 =
                [&](const size_t i, const uint32_t a, uint32_t& b, const uint32_t c, const uint32_t d, uint32_t& e)
            {
                e += std::rotl(a, 5) + block[i] + (d ^ (b & (c ^ d))) + 0x5a827999u;
                b = std::rotl(b, 30);
            };
            const auto r1 =
                [&](const size_t i, const uint32_t a, uint32_t& b, const uint32_t c, const uint32_t d, uint32_t& e)
            {
                e += std::rotl(a, 5) + extended(i) + (d ^ (b & (c ^ d))) + 0x5a827999u;
                b = std::rotl(b, 30);
            };
            const auto r2 =
                [&](const size_t i, const uint32_t a, uint32_t& b, const uint32_t c, const uint32_t d, uint32_t& e)
            {
                e += std::rotl(a, 5) + extended(i) + (b ^ c ^ d) + 0x6ed9eba1u;
                b = std::rotl(b, 30);
            };
            const auto r3 =
                [&](const size_t i, const uint32_t a, uint32_t& b, const uint32_t c, const uint32_t d, uint32_t& e)
            {
                e += std::rotl(a, 5) + extended(i) + ((b & c) | (d & (b ^ c))) + 0x8f1bbcdcu;
                b = std::rotl(b, 30);
            };
            const auto r4 =
                [&](const size_t i, const uint32_t a, uint32_t& b, const uint32_t c, const uint32_t d, uint32_t& e)
            {
                e += std::rotl(a, 5) + extended(i) + (b ^ c ^ d) + 0xca62c1d6u;
                b = std::rotl(b, 30);
            };

            uint32_t a = digest_[0], b = digest_[1], c = digest_[2], d = digest_[3], e = digest_[4];

            // clang-format off
            r0( 0, a, b, c, d, e); r0( 1, e, a, b, c, d); r0( 2, d, e, a, b, c); r0( 3, c, d, e, a, b); r0( 4, b, c, d, e, a);
            r0( 5, a, b, c, d, e); r0( 6, e, a, b, c, d); r0( 7, d, e, a, b, c); r0( 8, c, d, e, a, b); r0( 9, b, c, d, e, a);
            r0(10, a, b, c, d, e); r0(11, e, a, b, c, d); r0(12, d, e, a, b, c); r0(13, c, d, e, a, b); r0(14, b, c, d, e, a);
            r0(15, a, b, c, d, e); r1( 0, e, a, b, c, d); r1( 1, d, e, a, b, c); r1( 2, c, d, e, a, b); r1( 3, b, c, d, e, a);
            r2( 4, a, b, c, d, e); r2( 5, e, a, b, c, d); r2( 6, d, e, a, b, c); r2( 7, c, d, e, a, b); r2( 8, b, c, d, e, a);
            r2( 9, a, b, c, d, e); r2(10, e, a, b, c, d); r2(11, d, e, a, b, c); r2(12, c, d, e, a, b); r2(13, b, c, d, e, a);
            r2(14, a, b, c, d, e); r2(15, e, a, b, c, d); r2( 0, d, e, a, b, c); r2( 1, c, d, e, a, b); r2( 2, b, c, d, e, a);
            r2( 3, a, b, c, d, e); r2( 4, e, a, b, c, d); r2( 5, d, e, a, b, c); r2( 6, c, d, e, a, b); r2( 7, b, c, d, e, a);
            r3( 8, a, b, c, d, e); r3( 9, e, a, b, c, d); r3(10, d, e, a, b, c); r3(11, c, d, e, a, b); r3(12, b, c, d, e, a);
            r3(13, a, b, c, d, e); r3(14, e, a, b, c, d); r3(15, d, e, a, b, c); r3( 0, c, d, e, a, b); r3( 1, b, c, d, e, a);
            r3( 2, a, b, c, d, e); r3( 3, e, a, b, c, d); r3( 4, d, e, a, b, c); r3( 5, c, d, e, a, b); r3( 6, b, c, d, e, a);
            r3( 7, a, b, c, d, e); r3( 8, e, a, b, c, d); r3( 9, d, e, a, b, c); r3(10, c, d, e, a, b); r3(11, b, c, d, e, a);
            r4(12, a, b, c, d, e); r4(13, e, a, b, c, d); r4(14, d, e, a, b, c); r4(15, c, d, e, a, b); r4( 0, b, c, d, e, a);
            r4( 1, a, b, c, d, e); r4( 2, e, a, b, c, d); r4( 3, d, e, a, b, c); r4( 4, c, d, e, a, b); r4( 5, b, c, d, e, a);
            r4( 6, a, b, c, d, e); r4( 7, e, a, b, c, d); r4( 8, d, e, a, b, c); r4( 9, c, d, e, a, b); r4(10, b, c, d, e, a);
            r4(11, a, b, c, d, e); r4(12, e, a, b, c, d); r4(13, d, e, a, b, c); r4(14, c, d, e, a, b); r4(15, b, c, d, e, a);

            digest_[0] += a; digest_[1] += b; digest_[2] += c; digest_[3] += d; digest_[4] += e;
            // clang-format on
        }

        constexpr const auto& get_digest() noexcept
        {
            std::swap(digest_[0], digest_[4]);
            std::swap(digest_[1], digest_[3]);
            return digest_;
        }

    public:
        using hasher_crtp_impl::update;
        using hasher_crtp_impl::finalize;
    };

    class md5_hasher final : detail::hasher_crtp_impl<md5_hasher, std::endian::little>
    {
    private:
        friend hasher_crtp_impl;

        // clang-format off
        static constexpr std::array<int, 16> shift = {
            7, 12, 17, 22,
            5,  9, 14, 20,
            4, 11, 16, 23,
            6, 10, 15, 21
        };
        static constexpr std::array<uint32_t, 64> sines = {
            0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
            0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
            0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
            0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
            0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
            0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
            0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
            0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
            0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
            0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
            0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
            0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
            0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
            0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
            0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
            0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
        };
        // clang-format on

        std::array<uint32_t, 4> digest_{0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u};

        constexpr void process_block(const block32_t& block) noexcept
        {
            // SHA1 operations
            const auto r0 = [&](const size_t i, uint32_t& a, const uint32_t b, const uint32_t c, const uint32_t d)
            {
                const uint32_t f = (b & c) | (~b & d);
                a = b + std::rotl(f + a + sines[i] + block[i], shift[i % 4]);
            };
            const auto r1 = [&](const size_t i, uint32_t& a, const uint32_t b, const uint32_t c, const uint32_t d)
            {
                const uint32_t f = (d & b) | (~d & c);
                a = b + std::rotl(f + a + sines[i + 16] + block[(5 * i + 1) % 16], shift[i % 4 + 4]);
            };
            const auto r2 = [&](const size_t i, uint32_t& a, const uint32_t b, const uint32_t c, const uint32_t d)
            {
                const uint32_t f = b ^ c ^ d;
                a = b + std::rotl(f + a + sines[i + 32] + block[(3 * i + 5) % 16], shift[i % 4 + 8]);
            };
            const auto r3 = [&](const size_t i, uint32_t& a, const uint32_t b, const uint32_t c, const uint32_t d)
            {
                const uint32_t f = c ^ (b | ~d);
                a = b + std::rotl(f + a + sines[i + 48] + block[(7 * i) % 16], shift[i % 4 + 12]);
            };

            uint32_t a = digest_[0], b = digest_[1], c = digest_[2], d = digest_[3];

            // clang-format off
            r0( 0, a, b, c, d); r0( 1, d, a, b, c); r0( 2, c, d, a, b); r0( 3, b, c, d, a);
            r0( 4, a, b, c, d); r0( 5, d, a, b, c); r0( 6, c, d, a, b); r0( 7, b, c, d, a);
            r0( 8, a, b, c, d); r0( 9, d, a, b, c); r0(10, c, d, a, b); r0(11, b, c, d, a);
            r0(12, a, b, c, d); r0(13, d, a, b, c); r0(14, c, d, a, b); r0(15, b, c, d, a);
            r1( 0, a, b, c, d); r1( 1, d, a, b, c); r1( 2, c, d, a, b); r1( 3, b, c, d, a);
            r1( 4, a, b, c, d); r1( 5, d, a, b, c); r1( 6, c, d, a, b); r1( 7, b, c, d, a);
            r1( 8, a, b, c, d); r1( 9, d, a, b, c); r1(10, c, d, a, b); r1(11, b, c, d, a);
            r1(12, a, b, c, d); r1(13, d, a, b, c); r1(14, c, d, a, b); r1(15, b, c, d, a);
            r2( 0, a, b, c, d); r2( 1, d, a, b, c); r2( 2, c, d, a, b); r2( 3, b, c, d, a);
            r2( 4, a, b, c, d); r2( 5, d, a, b, c); r2( 6, c, d, a, b); r2( 7, b, c, d, a);
            r2( 8, a, b, c, d); r2( 9, d, a, b, c); r2(10, c, d, a, b); r2(11, b, c, d, a);
            r2(12, a, b, c, d); r2(13, d, a, b, c); r2(14, c, d, a, b); r2(15, b, c, d, a);
            r3( 0, a, b, c, d); r3( 1, d, a, b, c); r3( 2, c, d, a, b); r3( 3, b, c, d, a);
            r3( 4, a, b, c, d); r3( 5, d, a, b, c); r3( 6, c, d, a, b); r3( 7, b, c, d, a);
            r3( 8, a, b, c, d); r3( 9, d, a, b, c); r3(10, c, d, a, b); r3(11, b, c, d, a);
            r3(12, a, b, c, d); r3(13, d, a, b, c); r3(14, c, d, a, b); r3(15, b, c, d, a);

            digest_[0] += a; digest_[1] += b; digest_[2] += c; digest_[3] += d;
            // clang-format on
        }

        // Need to switch endian for MD5
        constexpr const auto& get_digest() noexcept
        {
            std::swap(digest_[0], digest_[3]);
            std::swap(digest_[1], digest_[2]);
            for (uint32_t& v : digest_)
                v = detail::switch_byte_order_u32(v);
            return digest_;
        }

    public:
        using hasher_crtp_impl::update;
        using hasher_crtp_impl::finalize;
    };

    template <hasher H>
    [[nodiscard]] constexpr digest_type<H> hash_buffer(const const_buffer bytes) noexcept
    {
        H h;
        h.update(bytes);
        return h.finalize();
    }

    [[nodiscard]] constexpr uint32_t fnv1a32(const const_buffer bytes) noexcept
    {
        return hash_buffer<fnv1a32_hasher>(bytes);
    }
    [[nodiscard]] constexpr uint64_t fnv1a64(const const_buffer bytes) noexcept
    {
        return hash_buffer<fnv1a64_hasher>(bytes);
    }
    [[nodiscard]] constexpr size_t fnv1a(const const_buffer bytes) noexcept { return hash_buffer<fnv1a_hasher>(bytes); }
    [[nodiscard]] constexpr auto sha1(const const_buffer bytes) noexcept { return hash_buffer<sha1_hasher>(bytes); }

    namespace detail
    {
        // Workaround for reinterpret_cast not being available at compile time
        template <hasher H>
        constexpr void constexpr_update_hash_sv(H& h, std::string_view sv) noexcept
        {
            if (std::is_constant_evaluated())
            {
                constexpr size_t chunk_size = 512;
                std::array<std::byte, chunk_size> byte_array{};
                const size_t length = sv.size();
                const size_t whole_chunk_count = length / chunk_size;
                const size_t leftover_size = length % chunk_size;
                for (size_t i = 0; i < whole_chunk_count; i++)
                {
                    for (size_t j = 0; j < chunk_size; j++)
                        byte_array[j] = static_cast<std::byte>(sv[j]);
                    sv.remove_prefix(chunk_size);
                    h.update({byte_array.data(), chunk_size});
                }
                for (size_t j = 0; j < leftover_size; j++)
                    byte_array[j] = static_cast<std::byte>(sv[j]);
                h.update({byte_array.data(), leftover_size});
            }
            else
                h.update(sv);
        }

        template <hasher H>
        constexpr auto constexpr_hash_sv(const std::string_view sv) noexcept
        {
            H h;
            constexpr_update_hash_sv(h, sv);
            return h.finalize();
        }
    } // namespace detail

    inline namespace literals
    {
        inline namespace hash_literals
        {
            [[nodiscard]] constexpr uint32_t operator""_fnv1a32(const char* bytes, const size_t length) noexcept
            {
                return detail::constexpr_hash_sv<fnv1a32_hasher>({bytes, length});
            }

            [[nodiscard]] constexpr uint64_t operator""_fnv1a64(const char* bytes, const size_t length) noexcept
            {
                return detail::constexpr_hash_sv<fnv1a64_hasher>({bytes, length});
            }

            [[nodiscard]] constexpr size_t operator""_fnv1a(const char* bytes, const size_t length) noexcept
            {
                return detail::constexpr_hash_sv<fnv1a_hasher>({bytes, length});
            }

            [[nodiscard]] constexpr auto operator""_sha1(const char* bytes, const size_t length) noexcept
            {
                return detail::constexpr_hash_sv<sha1_hasher>({bytes, length});
            }

            [[nodiscard]] constexpr auto operator""_md5(const char* bytes, const size_t length) noexcept
            {
                return detail::constexpr_hash_sv<md5_hasher>({bytes, length});
            }
        } // namespace hash_literals
    } // namespace literals

    inline constexpr uint32_t golden_ratio_32 = 0x9e3779b9u;
    inline constexpr uint64_t golden_ratio_64 = 0x9e3779b97f4a7c15ull;

    template <typename T>
    constexpr void hash_combine(size_t& seed, const T& value)
    {
        constexpr size_t golden_ratio = []
        {
            if constexpr (sizeof(size_t) == 4)
                return golden_ratio_32;
            else if constexpr (sizeof(size_t) == 8)
                return golden_ratio_64;
        }();
        seed ^= std::hash<T>{}(value) + golden_ratio + std::rotl(seed, 6) + std::rotr(seed, 2);
    }
} // namespace clu
