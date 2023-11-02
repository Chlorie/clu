#include "clu/uri.h"

#include <stack>
#include <array>
#include <span>
#include <limits>

#include "clu/parse.h"

namespace clu
{
    namespace
    {
        using namespace std::literals;

        constexpr std::size_t npos = std::string_view::npos;

        enum class percent_encode_type
        {
            must_encode = 0,
            unreserved,
            reserved
        };

        constexpr auto get_percent_encode_types() noexcept
        {
            constexpr std::size_t size = std::numeric_limits<unsigned char>::max() + 1;
            std::array<percent_encode_type, size> result{};
            for (const char ch : "!#$&'()*+,/:;=?@[]"sv)
                result[static_cast<unsigned char>(ch)] = percent_encode_type::reserved;
            for (const char ch : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"sv)
                result[static_cast<unsigned char>(ch)] = percent_encode_type::unreserved;
            return result;
        }
        constexpr auto percent_encode_types = get_percent_encode_types();

        [[noreturn]] void throw_bad_percent()
        {
            throw std::system_error(make_error_code(uri_errc::bad_percent_encoding));
        }

        unsigned char decode_one_percent(const char* ptr)
        {
            constexpr auto decode_one_char = [](const char ch) -> int
            {
                if (ch >= '0' && ch <= '9')
                    return ch - '0';
                if (ch >= 'a' && ch <= 'f')
                    return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F')
                    return ch - 'A' + 10;
                throw_bad_percent();
            };
            int result = decode_one_char(*++ptr);
            result = result * 16 + decode_one_char(*++ptr);
            return static_cast<unsigned char>(result);
        }

        void encode_one_percent(std::string& str, const unsigned char ch)
        {
            constexpr auto hex = "0123456789ABCDEF";
            const char percent[3]{'%', hex[ch / 16], hex[ch % 16]};
            str.append(percent, 3);
        }

        std::string appropriate_percent_encode(const std::string_view sv)
        {
            std::string result;
            result.reserve(sv.size());
            for (auto iter = sv.begin(); iter != sv.end(); ++iter)
            {
                if (*iter == '%')
                {
                    if (sv.end() - iter <= 2)
                        throw_bad_percent();
                    if (const unsigned char ch = decode_one_percent(&*iter);
                        percent_encode_types[ch] == percent_encode_type::unreserved)
                        result.push_back(static_cast<char>(ch));
                    else
                        encode_one_percent(result, ch);
                    iter += 2;
                    continue;
                }
                if (const auto ch = static_cast<unsigned char>(*iter);
                    percent_encode_types[ch] == percent_encode_type::must_encode)
                    encode_one_percent(result, ch);
                else
                    result.push_back(*iter);
            }
            return result;
        }

        std::size_t find(const std::string_view sv, const char ch, //
            const std::size_t offset = 0) noexcept
        {
            const std::size_t res = sv.find(ch, offset);
            return res == npos ? sv.size() : res;
        }

        bool starts_with(const std::string_view full, const std::string_view prefix)
        {
            return full.substr(0, prefix.size()) == prefix;
        }

        int parse_int(const std::string_view sv)
        {
            const auto result = clu::parse<int>(sv);
            if (!result)
                throw std::system_error(make_error_code(uri_errc::bad_port));
            return *result;
        }

        std::string remove_dots(std::string_view path)
        {
            std::stack<std::size_t> slashes;
            std::string result;
            const auto pop = [&]
            {
                if (!slashes.empty())
                {
                    result.resize(slashes.top());
                    slashes.pop();
                }
                else
                    result.clear();
            };
            while (!path.empty())
            {
                if (path == "." || path == "..")
                    return result;
                if (path == "/.")
                    return result + '/';
                if (path == "/..")
                {
                    pop();
                    return result + '/';
                }
                if (starts_with(path, "./") || starts_with(path, "/./"))
                    path.remove_prefix(2);
                else if (starts_with(path, "../"))
                    path.remove_prefix(3);
                else if (starts_with(path, "/../"))
                {
                    path.remove_prefix(3);
                    pop();
                }
                else
                {
                    const bool starts_with_slash = path[0] == '/';
                    slashes.push(result.size());
                    const auto next_slash = find(path, '/', starts_with_slash);
                    result += path.substr(0, next_slash);
                    path.remove_prefix(next_slash);
                }
            }
            return result;
        }

        std::string merge_paths(
            const std::string_view base, const std::string_view relative, const bool base_has_authority)
        {
            using namespace std::literals;
            if (base.empty() && base_has_authority)
                return "/"s += relative;
            if (const auto last_slash = base.find_last_of('/'); last_slash != npos)
                return std::string(base.substr(0, last_slash + 1)) += relative;
            return std::string(relative);
        }

        struct uri_error_category final : std::error_category
        {
            const char* name() const noexcept override { return "URI Error"; }

            std::string message(const int value) const override
            {
                using enum uri_errc;
                switch (static_cast<uri_errc>(value))
                {
                    case ok: return "ok";
                    case bad_port: return "bad port number";
                    case uri_not_absolute: return "base URI is not absolute";
                    case bad_percent_encoding: return "bad percent encoding";
                    default: return "unknown error";
                }
            }
        } const uri_error_category{};
    } // namespace

    bool operator==(const uri::component lhs, const std::string_view rhs) noexcept
    {
        if (lhs.undefined())
            return false;
        return *lhs == rhs;
    }

    bool operator==(const uri::component lhs, const uri::component rhs) noexcept
    {
        if (lhs.undefined() != rhs.undefined())
            return false;
        return *lhs == *rhs;
    }

    uri::uri(const std::string_view str): uri_(appropriate_percent_encode(str))
    {
        if (str.empty())
            return;

        std::size_t offset = 0;
        // Parse scheme
        if (const auto colon = uri_.find(':'); //
            colon != npos && full().substr(0, colon).find('/') == npos) // has scheme
        {
            scheme_ = {0, colon};
            offset = colon + 1;
        }

        // Parse authority
        if (starts_with(full().substr(offset), "//")) // has authority
        {
            offset += 2; // +2 for double slashes
            const auto slash = find(uri_, '/', offset);
            authority_ = {offset, slash};
            if (const auto at = uri_.find('@', offset); at < slash) // has userinfo
            {
                userinfo_ = {offset, at};
                offset = at + 1;
            }
            if (const auto colon = uri_.find(':', offset); colon < slash) // has port
            {
                port_view_ = {colon + 1, slash};
                if (const auto port_sv = *port_component(); !port_sv.empty())
                    port_ = parse_int(port_sv);
                host_ = {offset, colon};
            }
            else
                host_ = {offset, slash};
            offset = slash;
        }

        // Parse path
        {
            const auto path_delim = uri_.find_first_of("?#", offset); // find end of path
            if (const auto path_end = path_delim == npos ? uri_.size() : path_delim;
                authority_.undefined()) // no authority
                path_ = {offset, path_end};
            else if (path_end == offset) // has authority, empty path
            {
                uri_.insert(offset, 1, '/'); // Normalize to single slash
                path_ = {offset, offset + 1};
            }
            else // has authority, remove dots
            {
                const std::string_view path_view{&uri_[offset], path_end - offset};
                std::string new_path = remove_dots(path_view);
                uri_.replace(offset, path_end - offset, new_path);
                path_ = {offset, offset + new_path.size()};
            }
            offset = path_.stop;
        }

        // Parse query/fragment
        if (offset != uri_.size()) // has query and/or fragment
        {
            if (uri_[offset] == '?') // has query
            {
                const auto hash = find(uri_, '#', offset + 1);
                query_ = {offset + 1, hash};
                if (hash != uri_.size()) // has fragment
                    fragment_ = {hash + 1, uri_.size()};
            }
            else // only fragment
                fragment_ = {offset + 1, uri_.size()};
        }

        lower_case_component(scheme_);
        lower_case_component(host_);

        // Default ports
        if (port_ == -1 && is_absolute()) // default port for http and https
        {
            const auto sch = *scheme();
            if (sch == "http")
                port_ = 80;
            else if (sch == "https")
                port_ = 443;
        }
    }

    uri::component uri::origin() const noexcept
    {
        if (scheme_.undefined())
            return from_relative(authority_);
        if (authority_.undefined())
            return from_relative(scheme_);
        return from_relative({scheme_.start, authority_.stop});
    }

    std::string_view uri::path_and_query() const noexcept
    {
        if (query_.undefined())
            return path();
        return *from_relative({path_.start, query_.stop});
    }

    void uri::swap(uri& other) noexcept
    {
        std::swap(uri_, other.uri_);
        std::swap(scheme_, other.scheme_);
        std::swap(authority_, other.authority_);
        std::swap(userinfo_, other.userinfo_);
        std::swap(host_, other.host_);
        std::swap(port_view_, other.port_view_);
        std::swap(port_, other.port_);
        std::swap(path_, other.path_);
        std::swap(query_, other.query_);
        std::swap(fragment_, other.fragment_);
    }

    uri uri::resolve_relative(const uri& relative) const
    {
        if (!is_absolute())
            throw std::system_error(make_error_code(uri_errc::uri_not_absolute));

        // From RFC 3986
        uri result;
        if (relative.scheme())
        {
            result.append_scheme(relative.scheme());
            result.append_authority_like(relative);
            result.append_path(remove_dots(relative.path()));
            result.append_query(relative.query());
        }
        else
        {
            result.append_scheme(scheme());
            if (relative.authority())
            {
                result.append_authority_like(relative);
                if (result.port_ == -1)
                    result.port_ = port_;
                result.append_path(remove_dots(relative.path()));
                result.append_query(relative.query());
            }
            else
            {
                result.append_authority_like(*this);
                if (relative.path().empty())
                {
                    result.append_path(remove_dots(path()));
                    result.append_query((relative.query() ? relative : *this).query());
                }
                else
                {
                    if (const auto rel_path = relative.path(); rel_path[0] == '/')
                        result.append_path(remove_dots(rel_path));
                    else
                        result.append_path(remove_dots(merge_paths(path(), rel_path, !!authority())));
                    result.append_query(relative.query());
                }
            }
        }
        result.append_fragment(relative.fragment());
        return result;
    }

    void uri::lower_case_component(const relative_view rv) noexcept
    {
        if (rv.undefined())
            return;
        const std::span span{&uri_[rv.start], rv.size()};
        for (auto iter = span.begin(); iter != span.end(); ++iter)
        {
            char& ch = *iter;
            if (ch == '%')
            {
                iter += 2;
                continue;
            }
            if (ch >= 'A' && ch <= 'Z')
                ch += 32;
        }
    }

    uri::component uri::from_relative(const relative_view view) const noexcept
    {
        return view.undefined() //
            ? component{}
            : component{&uri_[view.start], &uri_[view.stop]};
    }

    void uri::append_scheme(const component comp)
    {
        if (!comp)
            return;
        const std::string_view sv = *comp;
        uri_ = sv;
        uri_ += ':';
        scheme_ = {0, sv.size()};
    }

    void uri::append_authority_like(const uri& other)
    {
        if (!other.authority())
            return;
        uri_ += "//";
        const std::size_t offset = other.authority_.start - uri_.size();
        const auto offset_relative = [offset](const relative_view view)
        {
            if (view.undefined())
                return view;
            return relative_view{view.start - offset, view.stop - offset};
        };
        authority_ = offset_relative(other.authority_);
        userinfo_ = offset_relative(other.userinfo_);
        host_ = offset_relative(other.host_);
        port_ = other.port_;
        uri_ += *other.authority();
    }

    void uri::append_path(const std::string_view sv)
    {
        path_ = {uri_.size(), uri_.size() + sv.size()};
        uri_ += sv;
    }

    void uri::append_query(const component comp)
    {
        if (!comp)
            return;
        const std::string_view sv = *comp;
        uri_ += '?';
        query_ = {uri_.size(), uri_.size() + sv.size()};
        uri_ += sv;
    }

    void uri::append_fragment(const component comp)
    {
        if (!comp)
            return;
        const std::string_view sv = *comp;
        uri_ += '#';
        fragment_ = {uri_.size(), uri_.size() + sv.size()};
        uri_ += sv;
    }

    std::error_code make_error_code(const uri_errc ec) { return {static_cast<int>(ec), uri_error_category}; }
} // namespace clu
