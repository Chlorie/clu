// Rough implementation of P1895R0:
// tag_invoke: A general pattern for supporting customisable functions

#pragma once

#include <type_traits>
#include <concepts>

namespace clu
{
    namespace detail
    {
        struct tag_invoke_t final
        {
            // @formatter:off
            template <typename Tag, typename... Args>
                requires requires(Tag tag, Args&&... args) { tag_invoke(tag, static_cast<Args&&>(args)...); }
            constexpr auto operator()(Tag tag, Args&&... args) const
                noexcept(noexcept(tag_invoke(tag, static_cast<Args&&>(args)...)))
                -> decltype(tag_invoke(tag, static_cast<Args&&>(args)...))
            {
                // You must write it twice twice.mp4
                return tag_invoke(tag, static_cast<Args&&>(args)...);
            }
            // @formatter:on
        };
    }

    inline namespace cpo_tag_invoke
    {
        inline constexpr detail::tag_invoke_t tag_invoke{};
    }

    template <auto& Tag> using tag_t = std::decay_t<decltype(Tag)>;

    template <typename Tag, typename... Args>
    concept tag_invocable = std::invocable<detail::tag_invoke_t, Tag, Args...>;

    // @formatter:off
    template <typename Tag, typename... Args>
    concept nothrow_tag_invocable =
        tag_invocable<Tag, Args...> && 
        std::is_nothrow_invocable_v<detail::tag_invoke_t, Tag, Args...>;
    // @formatter:on

    template <typename Tag, typename... Args>
    using tag_invoke_result = std::invoke_result<detail::tag_invoke_t, Tag, Args...>;
    template <typename Tag, typename... Args>
    using tag_invoke_result_t = std::invoke_result_t<detail::tag_invoke_t, Tag, Args...>;

#define CLU_DEFINE_TAG_INVOKE_CPO(cpo)                                                \
    inline constexpr struct cpo##_t final                                             \
    {                                                                                 \
        template <typename... Args> requires ::clu::tag_invocable<cpo##_t, Args&&...> \
        constexpr auto operator()(Args&&... args) const                               \
            noexcept(::clu::nothrow_tag_invocable<cpo##_t, Args&&...>)                \
            -> ::clu::tag_invoke_result_t<cpo##_t, Args&&...>                         \
        {                                                                             \
            return ::clu::tag_invoke(*this, static_cast<Args&&>(args)...);            \
        }                                                                             \
    } cpo{}
}
