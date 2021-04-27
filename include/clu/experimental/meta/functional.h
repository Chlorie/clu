#pragma once

#include <type_traits>
#include <concepts>

namespace clu::meta
{
    template <std::integral auto Value>
    using integral_constant = std::integral_constant<decltype(Value), Value>;

    template <typename T> struct is_integral_constant : std::false_type {};
    template <typename T, T V> struct is_integral_constant<std::integral_constant<T, V>> : std::true_type {};
    template <typename T> inline constexpr bool is_integral_constant_v = is_integral_constant<T>::value;

    // @formatter:off
    template <template <typename...> typename Func, typename... Ts>
    struct bind_front
    {
        template <typename... Us>
        struct call { using type = typename Func<Ts..., Us...>::type; };
    };

    template <template <typename...> typename Func, typename... Ts>
    struct bind_back
    {
        template <typename... Us>
        struct call { using type = typename Func<Us..., Ts...>::type; };
    };
    // @formatter:on

    template <typename T, typename U> struct plus {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct plus<T, U> : integral_constant<T::value + U::value> {};

    template <typename T, typename U> struct minus {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct minus<T, U> : integral_constant<T::value - U::value> {};

    template <typename T, typename U> struct multiplies {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct multiplies<T, U> : integral_constant<T::value * U::value> {};

    template <typename T, typename U> struct divides {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct divides<T, U> : integral_constant<T::value / U::value> {};

    template <typename T, typename U> struct modulus {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct modulus<T, U> : integral_constant<T::value % U::value> {};

    template <typename T> struct negate {};
    template <typename T> requires is_integral_constant_v<T> struct negate<T> : integral_constant<-T::value> {};

    template <typename T, typename U> struct equal_to {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct equal_to<T, U> : integral_constant<(T::value == U::value)> {};

    template <typename T, typename U> struct not_equal_to {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct not_equal_to<T, U> : integral_constant<(T::value != U::value)> {};

    template <typename T, typename U> struct less {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct less<T, U> : integral_constant<(T::value < U::value)> {};

    template <typename T, typename U> struct greater {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct greater<T, U> : integral_constant<(T::value > U::value)> {};

    template <typename T, typename U> struct less_equal {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct less_equal<T, U> : integral_constant<(T::value <= U::value)> {};

    template <typename T, typename U> struct greater_equal {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct greater_equal<T, U> : integral_constant<(T::value >= U::value)> {};

    template <typename T, typename U> struct logical_and {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct logical_and<T, U> : integral_constant<T::value && U::value> {};

    template <typename T, typename U> struct logical_or {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct logical_or<T, U> : integral_constant<T::value || U::value> {};

    template <typename T> struct logical_not {};
    template <typename T> requires is_integral_constant_v<T>
    struct logical_not<T> : integral_constant<!T::value> {};

    template <typename T, typename U> struct bit_and {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct bit_and<T, U> : integral_constant<T::value & U::value> {};

    template <typename T, typename U> struct bit_or {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct bit_or<T, U> : integral_constant<T::value | U::value> {};

    template <typename T, typename U> struct bit_xor {};
    template <typename T, typename U> requires (is_integral_constant_v<T> && is_integral_constant_v<U>)
    struct bit_xor<T, U> : integral_constant<T::value ^ U::value> {};

    template <typename T> struct bit_not {};
    template <typename T> requires is_integral_constant_v<T>
    struct bit_not<T> : integral_constant<~T::value> {};

    template <template <typename> typename UnaryFunc>
    struct not_fn
    {
        template <typename T>
        using call = typename logical_not<typename UnaryFunc<T>::type>::type;
    };
}
