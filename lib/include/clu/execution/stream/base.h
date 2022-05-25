#pragma once

#include "../algorithms/basic.h"

namespace clu::exec
{
    namespace detail::strm
    {
        struct next_t
        {
            template <typename S>
                requires tag_invocable<next_t, S&>
            auto operator()(S& stream) const //
                noexcept(nothrow_tag_invocable<next_t, S&>)
            {
                static_assert(sender<tag_invoke_result_t<next_t, S&>>, //
                    "next(stream) must produce a sender");
                return tag_invoke(*this, stream);
            }
        };

        struct cleanup_t
        {
            template <typename S>
            auto operator()(S& stream) const noexcept
            {
                if constexpr (tag_invocable<cleanup_t, S&>)
                {
                    static_assert(nothrow_tag_invocable<cleanup_t, S&>, //
                        "cleanup(stream) must not throw");
                    static_assert(sender<tag_invoke_result_t<cleanup_t, S&>>, //
                        "cleanup(stream) must produce a sender");
                    return tag_invoke(*this, stream);
                }
                else
                    return exec::just();
            }
        };
    } // namespace detail::strm

    using detail::strm::next_t;
    using detail::strm::cleanup_t;
    inline constexpr next_t next{};
    inline constexpr cleanup_t cleanup{};

    template <typename S>
    concept stream = callable<next_t, S&> && callable<cleanup_t, S&>;
} // namespace clu::exec
