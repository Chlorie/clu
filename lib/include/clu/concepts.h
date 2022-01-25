#pragma once

#include <concepts>

#include "type_traits.h"

namespace clu
{
    template <typename T, typename U>
    concept similar_to = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    template <typename Inv, typename Res, typename... Ts>
    concept invocable_of = std::invocable<Inv, Ts...> && std::convertible_to<std::invoke_result_t<Inv, Ts...>, Res>;

    template <typename F, typename... Args>
    concept callable = requires(F func, Args ... args) { static_cast<F&&>(func)(static_cast<Args&&>(args)...); };
    template <typename F, typename... Args>
    concept nothrow_callable = requires(F func, Args ... args) { { static_cast<F&&>(func)(static_cast<Args&&>(args)...) } noexcept; };
    template <typename F, typename... Args>
    using call_result_t = decltype(std::declval<F>()(std::declval<Args>()...));
    template <typename F, typename... Args>
    struct call_result : std::type_identity<call_result_t<F, Args...>> {};

    // Some exposition-only concepts in the standard, good to have them here
    // @formatter:off
    template <typename B>
    concept boolean_testable =
        std::convertible_to<B, bool> &&
        requires(B&& b) { { !static_cast<B&&>(b) } -> std::convertible_to<bool>; };

    template <typename T, typename U>
    concept weakly_equality_comparable_with =
        requires(const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u)
        {
            { t == u } -> boolean_testable;
            { t != u } -> boolean_testable;
            { u == t } -> boolean_testable;
            { u != t } -> boolean_testable;
        };

    template <typename T, typename U>
    concept partial_ordered_with =
        requires(const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u)
        {
            { t <  u } -> boolean_testable;
            { t >  u } -> boolean_testable;
            { t <= u } -> boolean_testable;
            { t <= u } -> boolean_testable;
            { u <  t } -> boolean_testable;
            { u >  t } -> boolean_testable;
            { u <= t } -> boolean_testable;
            { u <= t } -> boolean_testable;
        };

    template <typename T>
    concept movable_value = 
        std::move_constructible<std::decay_t<T>> &&
        std::constructible_from<std::decay_t<T>, T>;

    template <typename From, typename To> concept decays_to = std::same_as<std::decay_t<From>, To>;

    template <typename T> concept class_type = decays_to<T, T> && std::is_class_v<T>;

    template <typename T>
    concept nullable_pointer =
        std::default_initializable<T> &&
        std::copy_constructible<T> &&
        std::is_copy_assignable_v<T> &&
        std::equality_comparable<T> &&
        weakly_equality_comparable_with<T, std::nullptr_t>;
    // @formatter:on

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
    template <typename T, typename... Us>
    concept same_as_none_of = (!same_as_any_of<T, Us...>);

    // @formatter:off
    template <typename T, typename U>
    concept forwarding = 
        !std::is_rvalue_reference_v<T> &&
        std::same_as<std::remove_cvref_t<U>, U> &&
        std::same_as<std::remove_cvref_t<T>, U>;
    // @formatter:on

    template <typename T> concept enumeration = std::is_enum_v<T>;

    template <typename T> concept trivially_copyable = std::copyable<T> && std::is_trivially_copyable_v<T>;

    template <typename T, typename... Us>
    concept implicitly_constructible_from = requires(Us&&... args, void (* fptr)(const T&))
    {
        fptr({ static_cast<Us&&>(args)... });
    } && std::constructible_from<T, Us...>;
}
