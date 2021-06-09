// Rough implementation (with pipeables) of P0798R6: Monadic operations for std::optional

#pragma once

#include <optional>

#include "pipeable.h"

namespace clu
{
    namespace detail
    {
        template <typename Inv, typename Opt>
        using value_invoke_result_t = std::invoke_result_t<Inv, decltype(std::declval<Opt>().value())>;

        struct and_then_impl_t
        {
            template <typename Opt, typename Inv> requires
                template_of<std::remove_cvref_t<Opt>, std::optional> &&
                template_of<std::remove_cvref_t<value_invoke_result_t<Inv, Opt>>, std::optional>
            constexpr auto operator()(Opt&& optional, Inv&& invocable) const
            {
                using result_t = std::remove_cvref_t<value_invoke_result_t<Inv, Opt>>;
                if (optional)
                    return std::invoke(static_cast<Inv&&>(invocable), static_cast<Opt&&>(optional).value());
                else
                    return result_t();
            }
        };

        struct transform_impl_t
        {
            template <typename Opt, typename Inv> requires
                template_of<std::remove_cvref_t<Opt>, std::optional> &&
                std::invocable<Inv, decltype(std::declval<Opt>().value())>
            constexpr auto operator()(Opt&& optional, Inv&& invocable) const
            {
                using result_t = std::optional<value_invoke_result_t<Inv, Opt>>;
                if (optional)
                    return result_t(std::in_place,
                        std::invoke(static_cast<Inv&&>(invocable), static_cast<Opt&&>(optional).value()));
                else
                    return result_t();
            }
        };

        struct or_else_impl_t
        {
            template <typename Opt, typename Inv> requires
                template_of<std::remove_cvref_t<Opt>, std::optional> && (
                    std::convertible_to<std::invoke_result_t<Inv>, std::remove_cvref_t<Opt>> ||
                    std::is_void_v<std::invoke_result_t<Inv>>)
            constexpr std::remove_cvref_t<Opt> operator()(Opt&& optional, Inv&& invocable) const
            {
                if (optional)
                    return static_cast<Opt&&>(optional);
                else
                {
                    // This special case for void-returning invocables is removed in R5,
                    // but I think this idea is neat so I implemented it anyways.
                    if constexpr (std::is_void_v<std::invoke_result_t<Inv>>)
                    {
                        std::invoke(static_cast<Inv&&>(invocable));
                        return std::nullopt;
                    }
                    else
                        return std::invoke(static_cast<Inv&&>(invocable));
                }
            }
        };
    }

    inline constexpr auto and_then = pipeable(detail::and_then_impl_t());
    inline constexpr auto transform = pipeable(detail::transform_impl_t());
    inline constexpr auto or_else = pipeable(detail::or_else_impl_t());
}
