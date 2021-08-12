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
        (sha1_t{ 0xda39a3eeu, 0x5e6b4b0du, 0x3255bfefu, 0x95601890u, 0xafd80709u }));
    REQUIRE("The quick brown fox jumps over the lazy dog"_sha1 ==
        (sha1_t{ 0x2fd4e1c6u, 0x7a2d28fcu, 0xed849ee1u, 0xbb76e739u, 0x1b93eb12u }));

    // Compile time checks
    static_assert(""_sha1 
        == sha1_t{ 0xda39a3eeu, 0x5e6b4b0du, 0x3255bfefu, 0x95601890u, 0xafd80709u });
    static_assert("The quick brown fox jumps over the lazy dog"_sha1 
        == sha1_t{ 0x2fd4e1c6u, 0x7a2d28fcu, 0xed849ee1u, 0xbb76e739u, 0x1b93eb12u });
}
