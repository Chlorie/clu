#pragma once

#include <cstdint>

namespace clu
{
    template <typename... Types>
    struct type_list
    {
        static constexpr size_t size = sizeof...(Types);

        template <template <typename...> typename Templ>
        using apply = Templ<Types...>;
    };

    template <auto... values>
    struct value_list
    {
        static constexpr size_t size = sizeof...(values);

        template <template <auto...> typename Templ>
        using apply = Templ<values...>;
    };

    template <typename> struct extract_types {};
    template <template <typename...> typename Templ, typename... Types>
    struct extract_types<Templ<Types...>>
    {
        using type = type_list<Types...>;
    };
    template <typename Type> using extract_types_t = typename extract_types<Type>::type;
}
