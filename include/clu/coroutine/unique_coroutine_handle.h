#pragma once

#include <coroutine>
#include <utility>

namespace clu
{
    template <typename Pms = void>
    class unique_coroutine_handle
    {
    private:
        std::coroutine_handle<Pms> handle_;

    public:
        explicit unique_coroutine_handle(const std::coroutine_handle<Pms> hdl): handle_(hdl) {}
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

        std::coroutine_handle<Pms> get() const { return handle_; }
    };
}
