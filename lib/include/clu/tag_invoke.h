// Rough implementation of P1895R0:
// tag_invoke: A general pattern for supporting customisable functions

#pragma once

#include <type_traits>

#include "concepts.h"

namespace clu
{
    namespace detail
    {
        struct tag_invoke_t final
        {
            // @formatter:off
            template <typename Tag, typename... Args>
                requires requires(Tag tag, Args&&... args) { tag_invoke(tag, static_cast<Args&&>(args)...); }
            constexpr decltype(auto) operator()(Tag tag, Args&&... args) const
                noexcept(noexcept(tag_invoke(tag, static_cast<Args&&>(args)...)))
            {
                // You must write it twice twice.mp4
                return tag_invoke(tag, static_cast<Args&&>(args)...);
            }
            // @formatter:on
        };
    }

    inline constexpr detail::tag_invoke_t tag_invoke{};

    template <auto& Tag> using tag_t = std::decay_t<decltype(Tag)>;

    template <typename Tag, typename... Args>
    concept tag_invocable = callable<detail::tag_invoke_t, Tag, Args...>;

    // @formatter:off
    template <typename Tag, typename... Args>
    concept nothrow_tag_invocable =
        tag_invocable<Tag, Args...> && 
        nothrow_callable<detail::tag_invoke_t, Tag, Args...>;
    // @formatter:on

    template <typename Tag, typename... Args>
    using tag_invoke_result_t = call_result_t<detail::tag_invoke_t, Tag, Args...>;
    template <typename Tag, typename... Args>
    struct tag_invoke_result : std::type_identity<tag_invoke_result_t<Tag, Args...>> {};
}
