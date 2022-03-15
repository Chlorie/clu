#pragma once

#include <utility>

#include "coroutine.h"

namespace clu
{
    template <typename Pms = void>
    class unique_coroutine_handle
    {
    public:
        unique_coroutine_handle() = default;
        explicit unique_coroutine_handle(const coro::coroutine_handle<Pms> hdl): handle_(hdl) {}
        ~unique_coroutine_handle() noexcept
        {
            if (handle_)
                handle_.destroy();
        }
        unique_coroutine_handle(const unique_coroutine_handle&) = delete;
        unique_coroutine_handle(unique_coroutine_handle&& other) noexcept: handle_(std::exchange(other.handle_, {})) {}
        unique_coroutine_handle& operator=(const unique_coroutine_handle&) = delete;
        unique_coroutine_handle& operator=(unique_coroutine_handle&& other) noexcept
        {
            if (&other == this)
                return *this;
            if (handle_)
                handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
            return *this;
        }
        void swap(unique_coroutine_handle& other) noexcept { std::swap(handle_, other.handle_); }
        friend void swap(unique_coroutine_handle& lhs, unique_coroutine_handle& rhs) noexcept { lhs.swap(rhs); }

        coro::coroutine_handle<Pms> get() const noexcept { return handle_; }

        void resume() const
        {
            // libc++ doesn't allow resumptions on const handles
            // which is really absurd since the handles are copyable
            auto h = handle_;
            h.resume();
        }
        void operator()() const { resume(); }

        template <typename = int>
            requires(!std::is_void_v<Pms>)
        auto& promise() const noexcept { return handle_.promise(); }

        bool done() const noexcept { return handle_.done(); }

    private:
        coro::coroutine_handle<Pms> handle_{};
    };

    template <typename Pms>
    unique_coroutine_handle(coro::coroutine_handle<Pms>) -> unique_coroutine_handle<Pms>;
} // namespace clu
