// Rough implementation of P1895R0:
// tag_invoke: A general pattern for supporting customisable functions

#pragma once

#include <type_traits>

#include "concepts.h"
#include "macros.h"

namespace clu
{
    namespace detail::taginv
    {
        void tag_invoke();

        template <typename Tag, typename Obj, typename... Args>
        inline constexpr bool member_tag_invocable_impl = requires(Obj&& object, Tag&& tag, Args&&... args) {
            static_cast<Obj&&>(object).tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...);
        };
        template <typename Tag, typename... Args>
        concept member_tag_invocable = member_tag_invocable_impl<Tag, Args...>;
        template <typename Tag, typename... Args>
        concept adl_tag_invocable =
            requires(Tag&& tag, Args&&... args) { tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...); };

        template <typename Tag, typename... Args>
        concept tag_invocable = member_tag_invocable<Tag, Args...> || adl_tag_invocable<Tag, Args...>;

        struct tag_invoke_t
        {
            template <typename Tag, typename Obj, typename... Args>
                requires member_tag_invocable<Tag, Obj, Args...>
            constexpr CLU_STATIC_CALL_OPERATOR(decltype(auto))(Tag&& tag, Obj&& object, Args&&... args)
                CLU_SINGLE_RETURN(
                    static_cast<Obj&&>(object).tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...));

            template <typename Tag, typename... Args>
                requires adl_tag_invocable<Tag, Args...>
            constexpr CLU_STATIC_CALL_OPERATOR(decltype(auto))(Tag&& tag, Args&&... args)
                CLU_SINGLE_RETURN(tag_invoke(static_cast<Tag&&>(tag), static_cast<Args&&>(args)...));
        };

        template <typename Tag, typename... Args>
        concept nothrow_tag_invocable = tag_invocable<Tag, Args...> && nothrow_callable<tag_invoke_t, Tag, Args...>;

        template <typename Tag, typename... Args>
        using tag_invoke_result_t = call_result_t<tag_invoke_t, Tag, Args...>;

        template <typename Tag, typename... Args>
        struct tag_invoke_result
        {
        };
        template <typename Tag, typename... Args>
            requires tag_invocable<Tag, Args...>
        struct tag_invoke_result<Tag, Args...>
        {
            using type = call_result_t<tag_invoke_t, Tag, Args...>;
        };
    } // namespace detail::taginv

    inline namespace inl
    {
        inline constexpr detail::taginv::tag_invoke_t tag_invoke{};
    }

    using detail::taginv::tag_invocable;
    using detail::taginv::nothrow_tag_invocable;
    using detail::taginv::tag_invoke_result;
    using detail::taginv::tag_invoke_result_t;

    template <auto& Tag>
    using tag_t = std::decay_t<decltype(Tag)>;
} // namespace clu
