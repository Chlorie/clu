#pragma once

#include "type_list.h"

namespace clu::meta
{
    template <auto... Vs>
    struct value_list
    {
        static constexpr size_t size = sizeof...(Vs);
        using type = type_list<decltype(Vs)...>;
    };
    using empty_value_list = value_list<>;
}
