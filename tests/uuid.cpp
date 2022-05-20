#include <catch2/catch_test_macros.hpp>

#include "clu/uuid.h"

using namespace clu::uuid_literal;

TEST_CASE("uuid construction", "[uuid]")
{
    REQUIRE(clu::uuid().version() == clu::uuid::version_type::nil);
    REQUIRE(clu::uuid::generate_random().version() == clu::uuid::version_type::random);
    REQUIRE(clu::uuid::generate_name_based_md5(clu::uuid(), "").version() //
        == clu::uuid::version_type::name_based_md5);
    REQUIRE(clu::uuid(0x0123456789abcdefull, 0xfedcba9876543210ull).data() ==
        (clu::uuid::value_type{//
            std::byte(0x01), std::byte(0x23), std::byte(0x45), std::byte(0x67), //
            std::byte(0x89), std::byte(0xab), std::byte(0xcd), std::byte(0xef), //
            std::byte(0xfe), std::byte(0xdc), std::byte(0xba), std::byte(0x98), //
            std::byte(0x76), std::byte(0x54), std::byte(0x32), std::byte(0x10)}));
}

TEST_CASE("uuid string conversion", "[uuid]")
{
    const clu::uuid id{0xb2c06c36f2f652f0ull, 0xa0dfa6a2572bef96ull};
    SECTION("from string")
    {
        REQUIRE(clu::uuid::from_string("b2c06c36-f2f6-52f0-a0df-a6a2572bef96") == id);
        REQUIRE(clu::uuid::from_string("b2c06c36f2f652f0a0dfa6a2572bef96") == id);
        REQUIRE(clu::uuid::from_string("{b2c06c36-f2f6-52f0-a0df-a6a2572bef96}") == id);
        REQUIRE(clu::uuid::from_string("{b2c06c36f2f652f0a0dfa6a2572bef96}") == id);
    }
    SECTION("parse error")
    {
        REQUIRE_THROWS(clu::uuid::from_string("b2c0-6c36-f2f6-52f0-a0df-a6a2-572b-ef96"));
        REQUIRE_THROWS(clu::uuid::from_string("{b2c06c36-f2f6-52f0a0dfa6a2572bef96}"));
        REQUIRE_THROWS(clu::uuid::from_string("{b2c06c36-f2f6-52f0-a0df-a6a2572bef96"));
        REQUIRE_THROWS(clu::uuid::from_string("{b2c06c36f2f652f0a0dfa6a2572bef96deadbeaf}"));
    }
    SECTION("UDL")
    {
        REQUIRE("b2c06c36-f2f6-52f0-a0df-a6a2572bef96"_uuid == id);
        REQUIRE_THROWS("b2c06c36-f2f6-52f0-a0df-a6a2572bef96-"_uuid);
    }
    SECTION("round trip")
    {
        REQUIRE("b2c06c36-f2f6-52f0-a0df-a6a2572bef96"_uuid.to_string() //
            == "b2c06c36-f2f6-52f0-a0df-a6a2572bef96");
    }
}

TEST_CASE("uuid name based", "[uuid]")
{
    REQUIRE(clu::uuid::generate_name_based_sha1(clu::uuid_namespaces::url, "https://www.github.com") //
        == clu::uuid("b2c06c36-f2f6-52f0-a0df-a6a2572bef96"_uuid));
    REQUIRE(clu::uuid::generate_name_based_md5(clu::uuid_namespaces::url, "https://www.github.com") //
        == clu::uuid("92398e4e-0021-3723-b5a4-ec2550f246b2"_uuid));
}
