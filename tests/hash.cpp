#include <catch2/catch.hpp>
#include <clu/hash.h>

using namespace clu::literals;

TEST_CASE("FNV-1a", "[hash]")
{
    REQUIRE("\xcc\x24\x31\xc4"_fnv1a32 == 0u);
    REQUIRE("\xe0\x4d\x9f\xcb"_fnv1a32 == 0u);
    REQUIRE("\xd5\x6b\xb9\x53\x42\x87\x08\x36"_fnv1a64 == 0ull);
}

TEST_CASE("SHA-1", "[hash]")
{
    using sha1_t = clu::digest_type<clu::sha1_hasher>;

    // Runtime checks
    REQUIRE(""_sha1 ==
        (sha1_t{ 0xafd80709u, 0x95601890u, 0x3255bfefu, 0x5e6b4b0du, 0xda39a3eeu }));
    REQUIRE("The quick brown fox jumps over the lazy dog"_sha1 ==
        (sha1_t{ 0x1b93eb12u, 0xbb76e739u, 0xed849ee1u, 0x7a2d28fcu, 0x2fd4e1c6u }));

    // Compile time checks
    static_assert(""_sha1
        == sha1_t{ 0xafd80709u, 0x95601890u, 0x3255bfefu, 0x5e6b4b0du, 0xda39a3eeu });
    static_assert("The quick brown fox jumps over the lazy dog"_sha1
        == sha1_t{ 0x1b93eb12u, 0xbb76e739u, 0xed849ee1u, 0x7a2d28fcu, 0x2fd4e1c6u });
}

TEST_CASE("MD5", "[hash]")
{
    using md5_t = clu::digest_type<clu::md5_hasher>;

    // Runtime checks
    REQUIRE(""_md5 ==
        (md5_t{ 0xecf8427eu, 0xe9800998u, 0x8f00b204u, 0xd41d8cd9u }));
    REQUIRE("The quick brown fox jumps over the lazy dog"_md5 ==
        (md5_t{ 0x42a419d6u, 0x6bd81d35u, 0x372bb682u, 0x9e107d9du }));

    // Compile time checks
    static_assert(""_md5
        == md5_t{ 0xecf8427eu, 0xe9800998u, 0x8f00b204u, 0xd41d8cd9u });
    static_assert("The quick brown fox jumps over the lazy dog"_md5
        == md5_t{ 0x42a419d6u, 0x6bd81d35u, 0x372bb682u, 0x9e107d9du });
}
