#pragma once

#include <type_traits>

namespace clu
{
    template <auto... values> struct value_list;

    template <typename... Types>
    struct type_list
    {
        static constexpr auto size = sizeof...(Types);

        template <template <typename...> typename Templ>
        using apply = Templ<Types...>;
        template <template <typename...> typename Templ>
        using apply_t = typename Templ<Types...>::type;
        template <template <typename...> typename Templ>
        static constexpr auto apply_v = Templ<Types...>::value;

        template <typename... Others>
        using concatenate_front = type_list<Others..., Types...>;
        template <typename... Others>
        using concatenate_back = type_list<Types..., Others...>;

        template <typename Target>
        using contains = std::bool_constant<(std::is_same_v<Types, Target> || ...)>;
        template <typename Target>
        static constexpr bool contains_v = (std::is_same_v<Types, Target> || ...);
    };

    template <typename... Types> using to_value_list = value_list<Types::value...>;

    template <auto... values>
    struct value_list
    {
        static constexpr auto size = sizeof...(values);

        using as_type_list = type_list<value_tag_t<values>...>;
        using types = type_list<decltype(values)...>;

        template <template <auto...> typename Templ>
        using apply = Templ<values...>;
        template <template <auto...> typename Templ>
        using apply_t = typename Templ<values...>::type;
        template <template <auto...> typename Templ>
        static constexpr auto apply_v = Templ<values...>::value;
    };
}
