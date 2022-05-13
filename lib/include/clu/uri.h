#pragma once

#include <string>
#include <system_error>

#include "export.h"

namespace clu
{
    CLU_SUPPRESS_EXPORT_WARNING
    class CLU_API uri
    {
    public:
        class CLU_API component
        {
        public:
            [[nodiscard]] std::string_view get() const noexcept { return {begin_, end_}; }
            [[nodiscard]] std::string_view operator*() const noexcept { return {begin_, end_}; }
            [[nodiscard]] explicit operator std::string_view() const noexcept { return {begin_, end_}; }
            [[nodiscard]] bool empty() const noexcept { return begin_ && begin_ == end_; }
            [[nodiscard]] explicit operator bool() const noexcept { return !!begin_; }
            [[nodiscard]] bool undefined() const noexcept { return !begin_; }

            CLU_API friend bool operator==(component lhs, std::string_view rhs) noexcept;
            CLU_API friend bool operator==(component lhs, component rhs) noexcept;

        private:
            friend class uri;

            const char* begin_ = nullptr;
            const char* end_ = nullptr;

            constexpr component() = default;
            constexpr component(const char* begin, const char* end): begin_(begin), end_(end) {}
        };

        constexpr uri() noexcept = default;
        explicit uri(std::string str);

        [[nodiscard]] const std::string& full() const noexcept { return uri_; }

        [[nodiscard]] component scheme() const noexcept { return from_relative(scheme_); }
        [[nodiscard]] component authority() const noexcept { return from_relative(authority_); }
        [[nodiscard]] component userinfo() const noexcept { return from_relative(userinfo_); }
        [[nodiscard]] component host() const noexcept { return from_relative(host_); }
        [[nodiscard]] component origin() const noexcept;
        [[nodiscard]] component port_component() const noexcept { return from_relative(port_view_); }
        [[nodiscard]] int port() const noexcept { return port_; }
        [[nodiscard]] std::string_view path() const noexcept { return *from_relative(path_); }
        [[nodiscard]] component query() const noexcept { return from_relative(query_); }
        [[nodiscard]] component fragment() const noexcept { return from_relative(fragment_); }
        [[nodiscard]] std::string_view path_and_query() const noexcept;
        [[nodiscard]] std::string_view target() const noexcept { return *from_relative({path_.start, uri_.size()}); }

        [[nodiscard]] bool operator==(const uri& other) const noexcept { return uri_ == other.uri_; }
        [[nodiscard]] bool is_absolute() const noexcept { return !scheme().undefined(); }
        [[nodiscard]] bool empty() const noexcept { return uri_.empty(); }

        void swap(uri& other) noexcept;
        friend void swap(uri& lhs, uri& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] uri resolve_relative(const uri& relative) const;

    private:
        struct relative_view
        {
            static constexpr auto npos = static_cast<std::size_t>(-1);
            std::size_t start = npos;
            std::size_t stop = npos;
            bool undefined() const noexcept { return start == npos; }
            std::size_t size() const noexcept { return stop - start; }
        };

        std::string uri_;
        relative_view scheme_;
        relative_view authority_;
        relative_view userinfo_;
        relative_view host_;
        relative_view port_view_;
        int port_ = -1;
        relative_view path_{0, 0};
        relative_view query_;
        relative_view fragment_;

        component from_relative(relative_view view) const noexcept;

        // Modifiers for reference resolution
        void append_scheme(component comp);
        void append_authority_like(const uri& other);
        void append_path(std::string_view sv);
        void append_query(component comp);
        void append_fragment(component comp);
    };
    CLU_RESTORE_EXPORT_WARNING

    enum class uri_errc
    {
        ok = 0,
        bad_port,
        uri_not_absolute
    };

    CLU_API std::error_code make_error_code(uri_errc ec);
} // namespace clu

namespace clu::inline literals::inline uri_literal
{
    CLU_API [[nodiscard]] inline uri operator""_uri(const char* ptr, const std::size_t size)
    {
        return uri({ptr, size});
    }
} // namespace clu::inline literals::inline uri_literal

// clang-format off
template <> struct std::is_error_code_enum<clu::uri_errc> : std::true_type {};
// clang-format on
