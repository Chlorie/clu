#pragma once

#include <utility>

#include "coroutine.h"

namespace clu
{
    template <typename Pms = void>
    class unique_coroutine_handle
    {
    private:
        coro::coroutine_handle<Pms> handle_{};

    public:
        unique_coroutine_handle() = default;
        explicit unique_coroutine_handle(const coro::coroutine_handle<Pms> hdl): handle_(hdl) {}
        ~unique_coroutine_handle() noexcept { if (handle_) handle_.destroy(); }
        unique_coroutine_handle(const unique_coroutine_handle&) = delete;
        unique_coroutine_handle(unique_coroutine_handle&& other) noexcept: handle_(std::exchange(other.handle_, {})) {}
        unique_coroutine_handle& operator=(const unique_coroutine_handle&) = delete;
        unique_coroutine_handle& operator=(unique_coroutine_handle&& other) noexcept
        {
            if (&other == this) return *this;
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
            return *this;
        }
        void swap(unique_coroutine_handle& other) noexcept { std::swap(handle_, std::move(other.handle_)); }
        friend void swap(unique_coroutine_handle& lhs, unique_coroutine_handle& rhs) noexcept { lhs.swap(rhs); }

        coro::coroutine_handle<Pms> get() const { return handle_; }
    };

    template <typename Pms>
    unique_coroutine_handle(coro::coroutine_handle<Pms>) -> unique_coroutine_handle<Pms>;
}
