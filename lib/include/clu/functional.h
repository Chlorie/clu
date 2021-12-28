// Rough implementation (with pipeables) of P0798R6: Monadic operations for std::optional

#pragma once

#include <optional>

#include "piper.h"

namespace clu
{
    namespace detail
    {
        template <typename Inv, typename Opt>
        using value_invoke_result_t = std::invoke_result_t<Inv, decltype(std::declval<Opt>().value())>;
    }

    inline struct and_then_t
    {
        template <typename Opt, typename Inv> requires
            template_of<std::remove_cvref_t<Opt>, std::optional> &&
            template_of<std::remove_cvref_t<detail::value_invoke_result_t<Inv, Opt>>, std::optional>
        constexpr auto operator()(Opt&& optional, Inv&& invocable) const
        {
            using result_t = std::remove_cvref_t<detail::value_invoke_result_t<Inv, Opt>>;
            if (optional)
                return std::invoke(static_cast<Inv&&>(invocable), static_cast<Opt&&>(optional).value());
            else
                return result_t();
        }

        template <typename Inv>
        constexpr auto operator()(Inv&& invocable) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Inv&&>(invocable)));
        }
    } constexpr and_then{};

    inline struct transform_t
    {
        template <typename Opt, typename Inv> requires
            template_of<std::remove_cvref_t<Opt>, std::optional> &&
            std::invocable<Inv, decltype(std::declval<Opt>().value())>
        constexpr auto operator()(Opt&& optional, Inv&& invocable) const
        {
            using result_t = std::optional<detail::value_invoke_result_t<Inv, Opt>>;
            if (optional)
                return result_t(std::in_place,
                    std::invoke(static_cast<Inv&&>(invocable), static_cast<Opt&&>(optional).value()));
            else
                return result_t();
        }

        template <typename Inv>
        constexpr auto operator()(Inv&& invocable) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Inv&&>(invocable)));
        }
    } constexpr transform{};

    inline struct or_else_t
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

        template <typename Inv>
        constexpr auto operator()(Inv&& invocable) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Inv&&>(invocable)));
        }
    } constexpr or_else{};
}
