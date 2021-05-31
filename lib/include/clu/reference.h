#pragma once

#include <functional>

#include "concepts.h"

namespace clu
{
    // @formatter:off
    template <typename T> struct unwrap_reference_keeping { using type = T; };
    template <typename T> requires template_of<std::remove_cvref_t<T>, std::reference_wrapper>
    struct unwrap_reference_keeping<T> { using type = typename std::remove_cvref_t<T>::type&; };
    // @formatter:on

    template <typename T>
    using unwrap_reference_keeping_t = typename unwrap_reference_keeping<T>::type;
}
