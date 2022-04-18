#pragma once

#include <random>

#include "concepts.h"
#include "export.h"

namespace clu
{
    CLU_API std::mt19937& random_engine();
    CLU_API void reseed();
    CLU_API void reseed(std::mt19937::result_type seed);

    template <std::integral T>
    T randint(const T low, const T high)
    {
        // The standard missed these char types for some reason
        if constexpr (same_as_any_of<T, char, signed char, unsigned char>)
        {
            using int_type = std::conditional_t<std::is_signed_v<T>, int16_t, uint16_t>;
            std::uniform_int_distribution<int_type> dist(static_cast<int_type>(low), static_cast<int_type>(high));
            return static_cast<T>(dist(random_engine()));
        }
        else
        {
            std::uniform_int_distribution<T> dist(low, high);
            return dist(random_engine());
        }
    }

    template <std::floating_point T>
    T randfloat(const T low, const T high)
    {
        std::uniform_real_distribution<T> dist(low, high);
        return dist(random_engine());
    }
} // namespace clu
