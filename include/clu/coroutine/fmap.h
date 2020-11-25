#pragma once

#include <utility>

#include "concepts.h"
#include "../reference.h"
#include "../type_traits.h"

namespace clu
{
    namespace detail
    {
        template <typename A, typename F>
        class fmap_awaitable
        {
        private:
            A awaitable_;
            F func_;

            template <typename QualifiedAwaitable>
            class awaiter
            {
            private:
                using awaitable_t = unwrap_reference_keeping_t<QualifiedAwaitable>;
                using awaiter_t = awaiter_type_t<awaitable_t>;
                using func_t = copy_cvref_t<QualifiedAwaitable, F>;
                using parent_t = copy_cvref_t<QualifiedAwaitable, fmap_awaitable>;

                parent_t parent_;
                awaiter_t child_;

            public:
                explicit awaiter(parent_t parent):
                    parent_(static_cast<parent_t>(parent)),
                    child_(get_awaiter(static_cast<awaitable_t>(parent.awaitable_))) {}

                bool await_ready() { return child_.await_ready(); }
                decltype(auto) await_suspend(const std::coroutine_handle<> handle) { return child_.await_suspend(handle); }

                decltype(auto) await_resume()
                {
                    if constexpr (std::is_void_v<await_result_t<awaitable_t>>)
                        return std::invoke(static_cast<func_t>(parent_.func_));
                    else
                        return std::invoke(static_cast<func_t>(parent_.func_), child_.await_resume());
                }
            };

        public:
            template <typename A0, typename F0>
            fmap_awaitable(A0&& awaitable, F0&& func):
                awaitable_(std::forward<A0>(awaitable)),
                func_(std::forward<F0>(func)) {}

            auto operator co_await() & { return awaiter<A&>(*this); }
            auto operator co_await() const & { return awaiter<const A&>(*this); }
            auto operator co_await() && { return awaiter<A&&>(std::move(*this)); }
            auto operator co_await() const && { return awaiter<const A&&>(std::move(*this)); }
        };

        template <typename A, typename F>
        fmap_awaitable(A, F) -> fmap_awaitable<A, F>;
    }

    // @formatter:off
    template <typename A, typename F>
        requires awaitable<std::remove_cvref_t<A>>
    // @formatter:on
    auto fmap(A&& awaitable, F&& func)
    {
        return detail::fmap_awaitable(
            std::forward<A>(awaitable), std::forward<F>(func));
    }
}
