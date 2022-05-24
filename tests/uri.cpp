#include <catch2/catch_test_macros.hpp>

#include "clu/uri.h"

using namespace clu::uri_literal;
using namespace std::literals;

template <>
struct Catch::StringMaker<clu::uri>
{
    static std::string convert(const clu::uri& value) { return value.full(); }
}; // namespace Catch

TEST_CASE("uri construction", "[uri]")
{
    SECTION("empty")
    {
        const clu::uri uri;
        CHECK(uri.empty());
        CHECK(uri.scheme().undefined());
        CHECK(uri.authority().undefined());
        CHECK(uri.userinfo().undefined());
        CHECK(uri.host().undefined());
        CHECK(uri.origin().undefined());
        CHECK(uri.port_component().undefined());
        CHECK(uri.port() == -1);
        CHECK(uri.path().empty());
        CHECK(uri.query().undefined());
        CHECK(uri.fragment().undefined());
        CHECK(uri.path_and_query().empty());
        CHECK(uri.target().empty());
        CHECK(!uri.is_absolute());
    }

    SECTION("normal")
    {
        const clu::uri uri = "http://www.ietf.org/rfc/rfc2396.txt"_uri;
        CHECK(!uri.empty());
        CHECK(uri.scheme() == "http");
        CHECK(uri.authority() == "www.ietf.org");
        CHECK(uri.userinfo().undefined());
        CHECK(uri.host() == "www.ietf.org");
        CHECK(uri.origin() == "http://www.ietf.org");
        CHECK(uri.port_component().undefined());
        CHECK(uri.port() == 80);
        CHECK(uri.path() == "/rfc/rfc2396.txt");
        CHECK(uri.query().undefined());
        CHECK(uri.fragment().undefined());
        CHECK(uri.path_and_query() == "/rfc/rfc2396.txt");
        CHECK(uri.target() == "/rfc/rfc2396.txt");
        CHECK(uri.is_absolute());
    }

    SECTION("full")
    {
        // Example from Wikipedia
        const clu::uri uri =
            "https://john.doe@www.example.com:123/forum/questions/?tag=networking&order=newest#top"_uri;
        CHECK(!uri.empty());
        CHECK(uri.scheme() == "https");
        CHECK(uri.authority() == "john.doe@www.example.com:123");
        CHECK(uri.userinfo() == "john.doe");
        CHECK(uri.host() == "www.example.com");
        CHECK(uri.origin() == "https://john.doe@www.example.com:123");
        CHECK(uri.port_component() == "123");
        CHECK(uri.port() == 123);
        CHECK(uri.path() == "/forum/questions/");
        CHECK(uri.query() == "tag=networking&order=newest");
        CHECK(uri.fragment() == "top");
        CHECK(uri.path_and_query() == "/forum/questions/?tag=networking&order=newest");
        CHECK(uri.target() == "/forum/questions/?tag=networking&order=newest#top");
        CHECK(uri.is_absolute());
    }

    SECTION("normalization")
    {
        // Example from Wikipedia
        const clu::uri uri = "HTTPS://User@Example.COM/%7eFoo%2a/./bar/baz/../qux"_uri;
        CHECK(!uri.empty());
        CHECK(uri.scheme() == "https");
        CHECK(uri.authority() == "User@example.com");
        CHECK(uri.userinfo() == "User");
        CHECK(uri.host() == "example.com");
        CHECK(uri.origin() == "https://User@example.com");
        CHECK(uri.port_component().undefined());
        CHECK(uri.port() == 443);
        CHECK(uri.path() == "/~Foo%2A/bar/qux");
        CHECK(uri.query().undefined());
        CHECK(uri.fragment().undefined());
        CHECK(uri.path_and_query() == "/~Foo%2A/bar/qux");
        CHECK(uri.target() == "/~Foo%2A/bar/qux");
        CHECK(uri.is_absolute());

        const clu::uri empty_path = "https://example.com"_uri;
        CHECK(empty_path.path() == "/");
    }
}

TEST_CASE("relative resolution", "[uri]")
{
    // Examples from RFC 3986
    const clu::uri base = "http://a/b/c/d;p?q"_uri;

    SECTION("normal examples")
    {
        CHECK(base.resolve_relative("g:h"_uri) == "g:h"_uri);
        CHECK(base.resolve_relative("g"_uri) == "http://a/b/c/g"_uri);
        CHECK(base.resolve_relative("./g"_uri) == "http://a/b/c/g"_uri);
        CHECK(base.resolve_relative("g/"_uri) == "http://a/b/c/g/"_uri);
        CHECK(base.resolve_relative("/g"_uri) == "http://a/g"_uri);
        CHECK(base.resolve_relative("//g"_uri) == "http://g"_uri);
        CHECK(base.resolve_relative("?y"_uri) == "http://a/b/c/d;p?y"_uri);
        CHECK(base.resolve_relative("g?y"_uri) == "http://a/b/c/g?y"_uri);
        CHECK(base.resolve_relative("#s"_uri) == "http://a/b/c/d;p?q#s"_uri);
        CHECK(base.resolve_relative("g#s"_uri) == "http://a/b/c/g#s"_uri);
        CHECK(base.resolve_relative("g?y#s"_uri) == "http://a/b/c/g?y#s"_uri);
        CHECK(base.resolve_relative(";x"_uri) == "http://a/b/c/;x"_uri);
        CHECK(base.resolve_relative("g;x"_uri) == "http://a/b/c/g;x"_uri);
        CHECK(base.resolve_relative("g;x?y#s"_uri) == "http://a/b/c/g;x?y#s"_uri);
        CHECK(base.resolve_relative({}) == "http://a/b/c/d;p?q"_uri);
        CHECK(base.resolve_relative("."_uri) == "http://a/b/c/"_uri);
        CHECK(base.resolve_relative("./"_uri) == "http://a/b/c/"_uri);
        CHECK(base.resolve_relative(".."_uri) == "http://a/b/"_uri);
        CHECK(base.resolve_relative("../"_uri) == "http://a/b/"_uri);
        CHECK(base.resolve_relative("../g"_uri) == "http://a/b/g"_uri);
        CHECK(base.resolve_relative("../.."_uri) == "http://a/"_uri);
        CHECK(base.resolve_relative("../../"_uri) == "http://a/"_uri);
        CHECK(base.resolve_relative("../../g"_uri) == "http://a/g"_uri);
    }

    SECTION("abnormal examples")
    {
        // Extra dot-dot
        CHECK(base.resolve_relative("../../../g"_uri) == "http://a/g"_uri);
        CHECK(base.resolve_relative("../../../../g"_uri) == "http://a/g"_uri);
        // Names with dots
        CHECK(base.resolve_relative("/./g"_uri) == "http://a/g"_uri);
        CHECK(base.resolve_relative("/../g"_uri) == "http://a/g"_uri);
        CHECK(base.resolve_relative("g."_uri) == "http://a/b/c/g."_uri);
        CHECK(base.resolve_relative(".g"_uri) == "http://a/b/c/.g"_uri);
        CHECK(base.resolve_relative("g.."_uri) == "http://a/b/c/g.."_uri);
        CHECK(base.resolve_relative("..g"_uri) == "http://a/b/c/..g"_uri);
        // Unnecessary dot-segments
        CHECK(base.resolve_relative("./../g"_uri) == "http://a/b/g"_uri);
        CHECK(base.resolve_relative("./g/."_uri) == "http://a/b/c/g/"_uri);
        CHECK(base.resolve_relative("g/./h"_uri) == "http://a/b/c/g/h"_uri);
        CHECK(base.resolve_relative("g/../h"_uri) == "http://a/b/c/h"_uri);
        CHECK(base.resolve_relative("g;x=1/./y"_uri) == "http://a/b/c/g;x=1/y"_uri);
        CHECK(base.resolve_relative("g;x=1/../y"_uri) == "http://a/b/c/y"_uri);
        // Path-like constructs in query/fragment
        CHECK(base.resolve_relative("g?y/./x"_uri) == "http://a/b/c/g?y/./x"_uri);
        CHECK(base.resolve_relative("g?y/../x"_uri) == "http://a/b/c/g?y/../x"_uri);
        CHECK(base.resolve_relative("g#s/./x"_uri) == "http://a/b/c/g#s/./x"_uri);
        CHECK(base.resolve_relative("g#s/../x"_uri) == "http://a/b/c/g#s/../x"_uri);
        // Same scheme name
        CHECK(base.resolve_relative("http:g"_uri) == "http:g"_uri);
    }
}
