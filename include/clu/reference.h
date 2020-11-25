#pragma once

#include <functional>

#include "concepts.h"

namespace clu
{
    template <typename T>
    [[deprecated]] struct unwrap_reference
    {
        using type = T;
    };

    template <typename T>
    [[deprecated]] struct unwrap_reference<std::reference_wrapper<T>>
    {
        using type = T;
    };

    template <typename T> [[deprecated]] using unwrap_reference_t = typename unwrap_reference<T>::type;

    template <typename T>
    [[deprecated]] decltype(auto) unwrap(T& ref)
    {
        if constexpr (template_of<T, std::reference_wrapper>)
            return ref.get();
        else
            return ref;
    }

    // @formatter:off
    template <typename T> struct unwrap_reference_keeping { using type = T; };
    template <typename T> requires template_of<std::remove_cvref_t<T>, std::reference_wrapper>
    struct unwrap_reference_keeping<T> { using type = typename std::remove_cvref_t<T>::type; };
    // @formatter:on

    template <typename T>
    using unwrap_reference_keeping_t = typename unwrap_reference_keeping<T>::type;
}
