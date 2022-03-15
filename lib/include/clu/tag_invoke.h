// Rough implementation of P1895R0:
// tag_invoke: A general pattern for supporting customisable functions

#pragma once

#include <type_traits>

#include "concepts.h"

namespace clu
{
    namespace detail::taginv
    {
        void tag_invoke();

        // clang-format off
        template <typename Tag, typename... Args>
        concept tag_invocable =
            requires(Tag&& tag, Args&&... args)
            {
                tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...); 
            };
        
        template <typename Tag, typename... Args>
        concept nothrow_tag_invocable = 
            tag_invocable<Tag, Args...> && 
            requires(Tag&& tag, Args&&... args)
            {
                { tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...) } noexcept;
            };
        // clang-format on

        template <typename Tag, typename... Args>
        using tag_invoke_result_t = decltype(tag_invoke(std::declval<Tag>(), std::declval<Args>()...));

        template <typename Tag, typename... Args>
        struct tag_invoke_result
        {
        };
        template <typename Tag, typename... Args>
            requires tag_invocable<Tag, Args...>
        struct tag_invoke_result<Tag, Args...>
        {
            using type = tag_invoke_result_t<Tag, Args...>;
        };

        struct tag_invoke_t
        {
            template <typename Tag, typename... Args>
                requires tag_invocable<Tag, Args...>
            constexpr decltype(auto) operator()(Tag&& tag, Args&&... args) const
                noexcept(nothrow_tag_invocable<Tag, Args...>)
            {
                return tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...);
            }
        };
    } // namespace detail::taginv

    inline constexpr detail::taginv::tag_invoke_t tag_invoke{};
    using detail::taginv::tag_invocable;
    using detail::taginv::nothrow_tag_invocable;
    using detail::taginv::tag_invoke_result;
    using detail::taginv::tag_invoke_result_t;

    template <auto& Tag>
    using tag_t = std::decay_t<decltype(Tag)>;
} // namespace clu
