#pragma once

#include <random>
#include <ranges>
#include <vector>
#include <algorithm>
#include <numeric>

#include "concepts.h"
#include "assertion.h"
#include "memory.h"

namespace clu
{
    [[deprecated("Renamed as thread_rng()")]] std::mt19937& random_engine();
    std::mt19937& thread_rng(); ///< Get a thread-local Mersenne twister RNG seeded with the default random device.
    void reseed(); ///< Reseed the thread-local Mersenne twister with the default random device.
    void reseed(std::mt19937::result_type seed); ///< Reseed the thread-local Mersenne twister with a given seed.

    /**
     * \brief Generate a random integer in an inclusive range with a uniform distribution.
     * \tparam T Type of the integer.
     * \tparam G Type of the RNG. Defaults to ``std::mt19937``.
     * \param low Lower bound of the range.
     * \param high Upper bound of the range (inclusive).
     * \param rng The random number generator. Defaults to use the thread-local Mersenne twister.
     */
    template <std::integral T, std::uniform_random_bit_generator G = std::mt19937>
    T randint(const T low, const T high, G& rng = thread_rng())
    {
        // The standard missed these char types for some reason
        if constexpr (same_as_any_of<T, char, signed char, unsigned char>)
        {
            using int_type = std::conditional_t<std::is_signed_v<T>, std::int16_t, std::uint16_t>;
            std::uniform_int_distribution<int_type> dist(static_cast<int_type>(low), static_cast<int_type>(high));
            return static_cast<T>(dist(rng));
        }
        else
        {
            std::uniform_int_distribution<T> dist(low, high);
            return dist(rng);
        }
    }

    /**
     * \brief Generate a random floating point number in an inclusive range with a uniform distribution.
     * \tparam T Type of the floating point number.
     * \tparam G Type of the RNG. Defaults to ``std::mt19937``.
     * \param low Lower bound of the range.
     * \param high Upper bound of the range (inclusive).
     * \param rng The random number generator. Defaults to use the thread-local Mersenne twister.
     */
    template <std::floating_point T, std::uniform_random_bit_generator G = std::mt19937>
    T randfloat(const T low, const T high, G& rng = thread_rng())
    {
        std::uniform_real_distribution<T> dist(low, high);
        return dist(rng);
    }

    /**
     * \brief Sample n random elements from a population with replacements.
     * \param population The population range.
     * \param cumulative_weights The cumulative (inclusive scan) weights, where W[i] = sum(j=0..i) w[j].
     * \param output The output iterator.
     * \param n The amount of elements to output.
     * \param rng The random number generator. Defaults to use the thread-local Mersenne twister.
     * \return The end of the output region.
     */
    template <std::ranges::random_access_range R, std::ranges::random_access_range W, std::weakly_incrementable O,
        std::uniform_random_bit_generator G = std::mt19937>
        requires std::indirectly_copyable<std::ranges::iterator_t<R>, O> && arithmetic<std::ranges::range_value_t<W>>
    O cumulative_weighted_sample(
        R&& population, W&& cumulative_weights, O output, std::ranges::range_difference_t<R> n, G& rng = thread_rng())
    {
        namespace sr = std::ranges;
        using weight_type = sr::range_value_t<W>;
        CLU_ASSERT(sr::size(population) == sr::size(cumulative_weights), //
            "The size of population and that of weights should be equal.");
        CLU_ASSERT(!sr::empty(population), "The population should not be empty.");
        while (n-- != 0)
        {
            const weight_type pos = [&]
            {
                const weight_type high = *sr::rbegin(cumulative_weights);
                if constexpr (std::integral<weight_type>)
                    return clu::randint(static_cast<weight_type>(1), high, rng);
                else
                    return clu::randfloat(static_cast<weight_type>(0), high, rng);
            }();
            auto iter = sr::lower_bound(cumulative_weights, pos);
            *output++ = population[sr::distance(sr::begin(cumulative_weights), iter)];
        }
        return output;
    }

    /**
     * \brief Sample n random elements from a population without replacements.
     * \param population The population range.
     * \param weights The weights range. The weights don't need to be normalized, but must all be positive.
     * \param output The output iterator.
     * \param n The amount of elements to output. Must be less than the population size.
     * \param rng The random number generator. Defaults to use the thread-local Mersenne twister.
     * \param alloc The allocator for intermediate allocations.
     * \return The end of the output region.
     * \see Efraimidis, P. S., & Spirakis, P. G. (2006). Weighted random sampling with a reservoir.
     * Information processing letters, 97(5), 181-185.
     */
    template <std::ranges::random_access_range R, std::ranges::random_access_range W, std::weakly_incrementable O,
        std::uniform_random_bit_generator G = std::mt19937, allocator A = std::allocator<std::ranges::range_value_t<R>>>
        requires std::indirectly_copyable<std::ranges::iterator_t<R>, O> && arithmetic<std::ranges::range_value_t<W>>
    O weighted_sample_no_replacements(R&& population, W&& weights, O output, std::ranges::range_difference_t<R> n,
        G& rng = thread_rng(), A alloc = A{})
    {
        namespace sr = std::ranges;
        const auto pop_size = sr::size(population);
        CLU_ASSERT(pop_size == sr::size(weights), //
            "The size of population and that of weights should be equal.");
        CLU_ASSERT(pop_size != 0, "The population should not be empty.");
        CLU_ASSERT(n <= pop_size,
            "When sampling without replacements, the number of elements to sample "
            "should not exceed the population size");
        using weight_type = sr::range_value_t<W>;
        using size_type = sr::range_size_t<R>;
        using diff_type = sr::range_difference_t<R>;
        using key_type = conditional_t<std::is_integral_v<weight_type>, double, weight_type>;
        using pair_type = std::pair<size_type, key_type>;
        using pair_alloc_type = rebound_allocator_t<A, pair_type>;
        std::vector<pair_type, pair_alloc_type> key_index(pop_size, pair_alloc_type(alloc));
        for (size_type i = 0; i < pop_size; ++i)
        {
            const key_type key = std::log(clu::randfloat(static_cast<key_type>(0), static_cast<key_type>(1), rng)) /
                static_cast<key_type>(weights[i]);
            key_index[i] = {i, key};
        }
        std::ranges::partial_sort(key_index, std::next(sr::begin(key_index), n), std::greater<>{},
            [](const auto pair) { return pair.second; });
        for (diff_type i = 0; i < n; ++i)
            *output++ = population[key_index[i].first];
        return output;
    }
} // namespace clu
