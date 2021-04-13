#pragma once

#include <concepts>
#include <utility>
#include <typeinfo>

#include "type_traits.h"

namespace clu
{
    namespace detail
    {
        // recursion base, no type matches
        template <typename B, typename F>
        [[noreturn]] void polymorphic_visit_impl(B&&, F&&) { throw std::bad_cast(); }

        template <typename D, typename... Ds, typename B, typename F>
        auto polymorphic_visit_impl(B&& base, F&& func) -> std::invoke_result_t<F&&, copy_cvref_t<B&&, D>>
        {
            constexpr bool is_const = std::is_const_v<std::remove_reference_t<B>>;
            using qual_ptr = std::conditional_t<is_const, const D*, D*>;
            if (const auto ptr = dynamic_cast<qual_ptr>(std::addressof(base))) // successful cast
                return std::invoke(std::forward<F>(func), static_cast<copy_cvref_t<B&&, D>>(*ptr));
            else // try the next type
                return polymorphic_visit_impl<Ds...>(std::forward<B>(base), std::forward<F>(func));
        }
    }

    template <typename... Ds, typename B, typename F> requires
        (std::derived_from<Ds, std::remove_cvref_t<B>> && ...) &&
        (std::invocable<F&&, copy_cvref_t<B&&, Ds>> && ...) &&
        all_same_v<std::invoke_result_t<F&&, copy_cvref_t<B&&, Ds>>...>
    decltype(auto) polymorphic_visit(B&& base, F&& func)
    {
        return detail::polymorphic_visit_impl<Ds...>(
            std::forward<B>(base), std::forward<F>(func));
    }
}
