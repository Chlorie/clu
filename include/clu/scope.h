#pragma once

#include <utility>
#include <stdexcept>

namespace clu
{
    template <typename F>
    class scope_exit final
    {
    private:
        F func_;
    public:
        constexpr explicit scope_exit(F&& func): func_(std::move(func)) {}
        scope_exit(const scope_exit&) = delete;
        scope_exit(scope_exit&&) = delete;
        scope_exit& operator=(const scope_exit&) = delete;
        scope_exit& operator=(scope_exit&&) = delete;
        ~scope_exit() noexcept(noexcept(func_())) { func_(); }
    };
    template <typename F> scope_exit(F) -> scope_exit<F>;

    template <typename F>
    class scope_fail final
    {
    private:
        int exception_count_ = std::uncaught_exceptions();
        F func_;
    public:
        constexpr explicit scope_fail(F&& func): func_(std::move(func)) {}
        scope_fail(const scope_fail&) = delete;
        scope_fail(scope_fail&&) = delete;
        scope_fail& operator=(const scope_fail&) = delete;
        scope_fail& operator=(scope_fail&&) = delete;

        ~scope_fail() noexcept(noexcept(func_()))
        {
            if (std::uncaught_exceptions() > exception_count_)
                func_();
        }
    };
    template <typename F> scope_fail(F) -> scope_fail<F>;

    template <typename F>
    class scope_success final
    {
    private:
        int exception_count_ = std::uncaught_exceptions();
        F func_;
    public:
        constexpr explicit scope_success(F&& func): func_(std::move(func)) {}
        scope_success(const scope_success&) = delete;
        scope_success(scope_success&&) = delete;
        scope_success& operator=(const scope_success&) = delete;
        scope_success& operator=(scope_success&&) = delete;

        ~scope_success() noexcept(noexcept(func_()))
        {
            if (std::uncaught_exceptions() == exception_count_)
                func_();
        }
    };
    template <typename F> scope_success(F) -> scope_success<F>;
}
