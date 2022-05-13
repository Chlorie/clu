#include "clu/uri.h"

#include <stack>

#include "clu/parse.h"

namespace clu
{
    namespace
    {
        constexpr std::size_t npos = std::string_view::npos;

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
                switch (static_cast<uri_errc>(value))
                {
                    case uri_errc::ok: return "ok";
                    case uri_errc::bad_port: return "bad port number";
                    case uri_errc::uri_not_absolute: return "base URI is not absolute";
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

    uri::uri(std::string str): uri_(std::move(str))
    {
        if (uri_.empty())
            return;
        std::size_t offset = 0;
        if (const auto colon = uri_.find(':'); //
            colon != npos && full().substr(0, colon).find('/') == npos) // has scheme
        {
            scheme_ = {0, colon};
            offset = colon + 1;
        }
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
        const auto path_delim = uri_.find_first_of("?#", offset); // find end of path
        path_ = {offset, path_delim == npos ? uri_.size() : path_delim};
        offset = path_.stop;
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

    uri::component uri::from_relative(const relative_view view) const noexcept
    {
        return view.undefined() //
            ? component{}
            : component{uri_.data() + view.start, uri_.data() + view.stop};
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
