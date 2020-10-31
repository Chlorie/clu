#pragma once

#include <concepts>

namespace clu
{
    template <typename T, typename U>
    concept similar_to = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    template <typename Inv, typename Res, typename... Ts>
    concept invocable_of = std::invocable<Inv> && std::convertible_to<std::invoke_result_t<Inv, Ts...>, Res>;

    template <typename Type, template <typename...> typename Templ>
    struct is_template_of : std::false_type {};
    template <template <typename...> typename Templ, typename... Types>
    struct is_template_of<Templ<Types...>, Templ> : std::true_type {};
    template <typename Type, template <typename...> typename Templ>
    constexpr bool is_template_of_v = is_template_of<Type, Templ>::value;

    template <typename Type, template <typename...> typename Templ>
    concept template_of = is_template_of<Type, Templ>::value;
}
