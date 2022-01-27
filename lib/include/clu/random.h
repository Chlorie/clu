#pragma once

#include <random>

#include "concepts.h"

namespace clu
{
    namespace detail
    {
        class seed_generator
        {
        private:
            std::random_device dev_;

        public:
            template <typename It>
            void generate(It begin, It end)
            {
                for (; begin != end; ++begin)
                    *begin = dev_();
            }
        };
    }

    inline std::mt19937& random_engine()
    {
        thread_local std::mt19937 engine = []
        {
            detail::seed_generator seed_gen;
            return std::mt19937(seed_gen);
        }();
        return engine;
    }

    inline void reseed()
    {
        detail::seed_generator seed_gen;
        random_engine().seed(seed_gen);
    }

    inline void reseed(const std::mt19937::result_type seed) { random_engine().seed(seed); }

    template <std::integral T>
    T randint(const T low, const T high)
    {
        // The standard missed these char types for some reason
        if constexpr (same_as_any_of<T, char, signed char, unsigned char>)
        {
            using int_type = std::conditional_t<
                std::is_signed_v<T>, int16_t, uint16_t>;
            std::uniform_int_distribution<int_type> dist(
                static_cast<int_type>(low), static_cast<int_type>(high));
            return static_cast<T>(dist(random_engine()));
        }
        else
        {
            std::uniform_int_distribution<T> dist(low, high);
            return dist(random_engine());
        }
    }
}