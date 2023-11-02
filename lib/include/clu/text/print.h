#pragma once

#include <cstdio>
#include <format>

namespace clu
{
    void print_nonformatted(std::FILE* file, std::string_view text);
    inline void print_nonformatted(const std::string_view text) { print_nonformatted(stdout, text); }

    inline void vprint(std::FILE* file, const std::string_view fmt, const std::format_args args)
    {
        print_nonformatted(file, std::vformat(fmt, args));
    }

    inline void vprint(const std::string_view fmt, const std::format_args args) { vprint(stdout, fmt, args); }

    template <typename... Args>
    void print(std::FILE* file, const std::format_string<Args...> fmt, Args&&... args)
    {
        clu::vprint(file, fmt.get(), std::make_format_args(static_cast<Args&&>(args)...));
    }

    template <typename... Args>
    void print(const std::format_string<Args...> fmt, Args&&... args)
    {
        clu::print(stdout, fmt, static_cast<Args&&>(args)...);
    }

    template <typename... Args>
    void println(std::FILE* file, std::format_string<Args...> fmt, Args&&... args)
    {
        std::string text = std::vformat(fmt.get(), std::make_format_args(static_cast<Args&&>(args)...));
        text.push_back('\n');
        print_nonformatted(file, text);
    }

    template <typename... Args>
    void println(const std::format_string<Args...> fmt, Args&&... args)
    {
        clu::println(stdout, fmt, static_cast<Args&&>(args)...);
    }
} // namespace clu
