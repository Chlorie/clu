#pragma once

#include <concepts>

namespace clu
{
    template <typename T, typename U>
    concept similar_to = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    template <typename Inv, typename Res, typename... Ts>
    concept invocable_of = std::invocable<Inv, Ts...> && std::convertible_to<std::invoke_result_t<Inv, Ts...>, Res>;

    template <typename Type, template <typename...> typename Templ>
    struct is_template_of : std::false_type {};
    template <template <typename...> typename Templ, typename... Types>
    struct is_template_of<Templ<Types...>, Templ> : std::true_type {};
    template <typename Type, template <typename...> typename Templ>
    constexpr bool is_template_of_v = is_template_of<Type, Templ>::value;

    template <typename Type, template <typename...> typename Templ>
    concept template_of = is_template_of<Type, Templ>::value;

    template <typename T, typename... Us>
    concept same_as_any_of = (std::same_as<T, Us> || ...);

    // @formatter:off
    template <typename T, typename U>
    concept forwarding = !std::is_rvalue_reference_v<T>
        && std::same_as<std::remove_cvref_t<U>, U>
        && std::same_as<std::remove_cvref_t<T>, U>;
    // @formatter:on

    template <typename T> concept enumeration = std::is_enum_v<T>;

    template <typename T> concept trivially_copyable = std::copyable<T> && std::is_trivially_copyable_v<T>;

    template <typename T, typename... Us>
    concept implicitly_constructible_from = requires(Us&&... args, void (* fptr)(const T&))
    {
        fptr({ static_cast<Us&&>(args)... });
    } && std::constructible_from<T, Us...>;
}
