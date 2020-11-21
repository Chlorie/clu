#pragma once

#include <type_traits>

namespace clu::meta
{
    // @formatter:off
    template <typename... Ts> struct type_list { static constexpr size_t size = sizeof...(Ts); };

    inline constexpr size_t npos = static_cast<size_t>(-1);

    template <size_t I, typename T>
    struct indexed_type
    {
        static constexpr size_t index = I;
        using type = T;
    };

    template <typename L> struct list_size {};
    template <typename... Ts> struct list_size<type_list<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};
    template <typename L> inline constexpr size_t list_size_v = list_size<L>::value;

    template <typename L> struct extract_list {};
    template <template <typename...> typename Templ, typename... Ts> struct extract_list<Templ<Ts...>> { using type = type_list<Ts...>; };
    template <typename T> using extract_list_t = typename extract_list<T>::type;

    template <typename L, template <typename...> typename Templ> struct apply {};
    template <typename... Ts, template <typename...> typename Templ> struct apply<type_list<Ts...>, Templ> { using type = Templ<Ts...>; };
    template <typename L, template <typename...> typename Templ> using apply_t = typename apply<L, Templ>::type;

    template <typename L> struct front {};
    template <typename T, typename... Rest> struct front<type_list<T, Rest...>> { using type = T; };
    template <typename L> using front_t = typename front<L>::type;

    template <typename L, typename M> struct concatenate {};
    template <typename... Ts, typename... Us>
    struct concatenate<type_list<Ts...>, type_list<Us...>> { using type = type_list<Ts..., Us...>; };
    template <typename L, typename M> using concatenate_t = typename concatenate<L, M>::type;
    // @formatter:on
}
