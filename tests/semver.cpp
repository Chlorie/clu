#include <catch2/catch.hpp>
#include <clu/semver.h>

using namespace clu::literals;

TEST_CASE("semver validate extra", "[semver]")
{
    SECTION("success")
    {
        REQUIRE_NOTHROW(clu::semver(1, 0, 0));
        REQUIRE_NOTHROW(clu::semver(1, 0, 0, "", "000"));
        REQUIRE_NOTHROW(clu::semver(1, 0, 0, "alpha.1-234", "beta123.hello"));
    }
    SECTION("empty id")
    {
        REQUIRE_THROWS(clu::semver(1, 0, 0, "."));
        REQUIRE_THROWS(clu::semver(1, 0, 0, "1."));
        REQUIRE_THROWS(clu::semver(1, 0, 0, ".1"));
        REQUIRE_THROWS(clu::semver(1, 0, 0, "", "."));
    }
    SECTION("invalid character")
    {
        REQUIRE_THROWS(clu::semver(1, 0, 0, "?"));
    }
    SECTION("leading zero")
    {
        REQUIRE_THROWS(clu::semver(1, 0, 0, "01"));
        REQUIRE_NOTHROW(clu::semver(1, 0, 0, "", "01")); // Build number is fine with leading zeros
    }
}

TEST_CASE("semver parsing", "[semver]")
{
    SECTION("success")
    {
        REQUIRE(clu::semver::from_string("0.1.0") == clu::semver(0, 1, 0));
        REQUIRE(clu::semver::from_string("1.0.0") == clu::semver(1, 0, 0));
        REQUIRE(clu::semver::from_string("1.17.2") == clu::semver(1, 17, 2));
        REQUIRE(clu::semver::from_string("1.0.0-alpha") == clu::semver(1, 0, 0, "alpha"));
        REQUIRE(clu::semver::from_string("1.0.0+build23") == clu::semver(1, 0, 0, "", "build23"));
        REQUIRE(clu::semver::from_string("1.0.0-alpha+build23") == clu::semver(1, 0, 0, "alpha", "build23"));
    }
    SECTION("empty extra versions")
    {
        REQUIRE_THROWS(clu::semver::from_string("1.0.0-"));
        REQUIRE_THROWS(clu::semver::from_string("1.0.0-alpha+"));
        REQUIRE_THROWS(clu::semver::from_string("1.0.0+"));
    }
    SECTION("leading zeros")
    {
        REQUIRE_THROWS(clu::semver::from_string("1.01.0"));
        REQUIRE_THROWS(clu::semver::from_string("0.0.01"));
    }
    SECTION("invalid characters")
    {
        REQUIRE_THROWS(clu::semver::from_string("1.0.-1"));
    }
    SECTION("incomplete")
    {
        REQUIRE_THROWS(clu::semver::from_string("1.0"));
        REQUIRE_THROWS(clu::semver::from_string("0"));
    }
    SECTION("prerelease version error")
    {
        REQUIRE_THROWS(clu::semver::from_string("1.0.0-pre2."));
        REQUIRE_THROWS(clu::semver::from_string("1.0.0-alpha.01"));
    }
    SECTION("build version error")
    {
        REQUIRE_THROWS(clu::semver::from_string("1.0.0+no!"));
        REQUIRE_THROWS(clu::semver::from_string("1.0.0+0."));
    }
    SECTION("using literals")
    {
        using namespace clu::literals;
        REQUIRE("1.0.0-alpha+build23"_semver == clu::semver(1, 0, 0, "alpha", "build23"));
        REQUIRE_THROWS("1.0.0+no!"_semver);
    }
}

TEST_CASE("semver comparison", "[semver]")
{
    SECTION("equality")
    {
        REQUIRE("1.0.0"_semver == "1.0.0"_semver);
        REQUIRE("1.0.0"_semver != "1.1.0"_semver);
        REQUIRE("1.0.0-alpha"_semver == "1.0.0-alpha"_semver);
        REQUIRE("1.0.0-alpha"_semver != "1.0.0-beta"_semver);
        REQUIRE("1.0.0-alpha+build1"_semver == "1.0.0-alpha+build1"_semver);
        REQUIRE("1.0.0-alpha+build1"_semver != "1.0.0-alpha+build2"_semver); // not equal
    }
    SECTION("equivalence")
    {
        REQUIRE(("1.0.0"_semver <=> "1.0.0"_semver == 0));
        REQUIRE(("1.0.0"_semver <=> "1.1.0"_semver != 0));
        REQUIRE(("1.0.0-alpha"_semver <=> "1.0.0-alpha"_semver == 0));
        REQUIRE(("1.0.0-alpha+build1"_semver <=> "1.0.0-alpha+build1"_semver == 0));
        REQUIRE(("1.0.0-alpha+build1"_semver <=> "1.0.0-alpha+build2"_semver == 0)); // but equivalent
    }
    SECTION("different major/minor/patch")
    {
        REQUIRE("1.0.0"_semver < "2.0.0"_semver);
        REQUIRE("1.16.0"_semver < "2.0.1"_semver);
        REQUIRE("0.0.1"_semver < "0.1.0"_semver);
    }
    SECTION("comparing prereleases")
    {
        REQUIRE("1.0.0"_semver > "1.0.0-rc.1"_semver); // release is greater than prerelease
        REQUIRE("1.0.0"_semver < "1.1.0-rc.1"_semver); // comparing release versions
        REQUIRE("1.0.0-alpha"_semver < "1.0.0-beta"_semver); // lexicographical comparison
        REQUIRE("1.0.0-alpha"_semver < "1.0.0-alpha.1"_semver); // longer ones are greater
        REQUIRE("1.0.0-1"_semver < "1.0.0-alpha"_semver); // numeric compares less
        REQUIRE("1.0.0-2"_semver < "1.0.0-10"_semver); // pure numeric parts compare as integers
        REQUIRE("1.0.0-rc.2"_semver < "1.0.0-rc.10"_semver); // mixed
    }
    SECTION("semver.org examples")
    {
        REQUIRE("1.0.0-alpha"_semver < "1.0.0-alpha.1"_semver);
        REQUIRE("1.0.0-alpha.1"_semver < "1.0.0-alpha.beta"_semver);
        REQUIRE("1.0.0-alpha.beta"_semver < "1.0.0-beta"_semver);
        REQUIRE("1.0.0-beta"_semver < "1.0.0-beta.2"_semver);
        REQUIRE("1.0.0-beta.2"_semver < "1.0.0-beta.11"_semver);
        REQUIRE("1.0.0-beta.11"_semver < "1.0.0-rc.1"_semver);
        REQUIRE("1.0.0-rc.1"_semver < "1.0.0"_semver);
    }
}

TEST_CASE("semver string conversion", "[semver]")
{
    for (const auto ver : {
             "1.0.0-alpha",
             "1.0.0-alpha.1",
             "1.0.0-alpha.beta",
             "1.0.0-beta",
             "1.0.0-beta.2",
             "1.0.0-beta.11",
             "1.0.0-rc.1",
             "1.0.0",
         })
        REQUIRE(clu::semver::from_string(ver).to_string() == ver);
}
