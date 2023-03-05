#pragma once

namespace clu
{
    template <typename... Types>
    struct type_list
    {
        static constexpr auto size = sizeof...(Types);
    };

    template <auto... values>
    struct value_list
    {
        static constexpr auto size = sizeof...(values);
    };
} // namespace clu
