#pragma once

#include "type_traits.h"

namespace clu
{
    template <auto... values> struct value_list;

    template <typename... Types>
    struct type_list
    {
        static constexpr auto size = sizeof...(Types);
    };

    template <typename... Types> using to_value_list = value_list<Types::value...>;

    template <auto... values>
    struct value_list
    {
        static constexpr auto size = sizeof...(values);
    };
}
