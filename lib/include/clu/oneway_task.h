#pragma once

#include <coroutine>
#include <exception>

namespace clu
{
    /// A fire-and-forget coroutine type.
    ///
    /// The coroutine is started inline eagerly. Any uncaught exception thrown
    /// in the coroutine will trigger `std::terminate`.
    struct oneway_task final
    {
        struct promise_type final
        {
            oneway_task get_return_object() const noexcept { return {}; }
            std::suspend_never initial_suspend() const noexcept { return {}; }
            std::suspend_never final_suspend() const noexcept { return {}; }
            void return_void() const noexcept {}
            void unhandled_exception() const noexcept { std::terminate(); }
        };
    };
}
