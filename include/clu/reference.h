#pragma once

#include <functional>

namespace clu
{
    template <typename T>
    struct unwrap_reference
    {
        using type = T;
    };

    template <typename T>
    struct unwrap_reference<std::reference_wrapper<T>>
    {
        using type = T;
    };

    template <typename T> using unwrap_reference_t = typename unwrap_reference<T>::type;

    template <typename T>
    decltype(auto) unwrap(T& ref)
    {
        if constexpr (template_of<T, std::reference_wrapper>)
            return ref.get();
        else
            return ref;
    }
}
