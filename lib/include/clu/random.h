#pragma once

#include <random>
#include <ranges>
#include <vector>
#include <algorithm>
#include <numeric>
#include <bit>

import clu.core;
#include "assertion.h"
#include "memory.h"

namespace clu
{
    template <typename Seq, typename Engine>
    concept seed_sequence_for = (!std::same_as<Seq, Engine>)&&(!std::same_as<Seq, typename Engine::result_type>);

    /// \brief Simple and fast PRNG with a 64-bit state.
    class splitmix64 final
    {
    public:
        using result_type = u64;

        constexpr splitmix64() noexcept = default;
        constexpr explicit splitmix64(const u64 seed) noexcept: state_(seed) {}
        constexpr explicit splitmix64(seed_sequence_for<splitmix64> auto& seed_seq) { this->seed(seed_seq); }

        constexpr void seed(const u64 seed) noexcept { state_ = seed; }

        constexpr void seed(seed_sequence_for<splitmix64> auto& seed_seq)
        {
            u32 s[2];
            seed_seq.generate(std::begin(s), std::end(s));
            state_ = std::bit_cast<u64, u32[2]>(s);
        }

        constexpr friend bool operator==(splitmix64, splitmix64) noexcept = default;

        constexpr static u64 min() noexcept { return 0; }
        constexpr static u64 max() noexcept { return static_cast<u64>(-1); }

        constexpr u64 operator()() noexcept
        {
            u64 res = (state_ += 0x9e3779b97f4a7c15_u64);
            res = (res ^ (res >> 30)) * 0xbf58476d1ce4e5b9;
            res = (res ^ (res >> 27)) * 0x94d049bb133111eb;
            return res ^ (res >> 31);
        }

    private:
        u64 state_ = 0x8badf00ddeadbeef_u64;
    };
    static_assert(std::uniform_random_bit_generator<splitmix64>);

    /// \brief xoshiro256**, a fast and solid PRNG with a 256-bit state.
    class xoshiro256 final
    {
    public:
        using result_type = u64;

        constexpr xoshiro256() noexcept: xoshiro256(0) {}
        constexpr explicit xoshiro256(const u64 seed) noexcept: state_(seed) {}
        constexpr explicit xoshiro256(seed_sequence_for<xoshiro256> auto& seed_seq) { this->seed(seed_seq); }

        constexpr void seed(const u64 seed) noexcept { state_ = state(seed); }

        constexpr void seed(seed_sequence_for<xoshiro256> auto& seed_seq)
        {
            u32 s[8];
            seed_seq.generate(std::begin(s), std::end(s));
            state_ = std::bit_cast<state, u32[8]>(s);
        }

        constexpr bool operator==(const xoshiro256&) const noexcept = default;

        constexpr static u64 min() noexcept { return 0; }
        constexpr static u64 max() noexcept { return static_cast<u64>(-1); }

        constexpr u64 operator()() noexcept { return state_.next(); }

    private:
        struct state
        {
            u64 data[4];

            constexpr state() noexcept = default;

            constexpr explicit state(const u64 seed) noexcept
            {
                splitmix64 s(seed);
                data[0] = s();
                data[1] = s();
                data[2] = s();
                data[3] = s();
            }

            constexpr bool operator==(const state&) const noexcept = default;

            constexpr u64 next() noexcept
            {
                const u64 res = std::rotl(data[1] * 5, 7) * 9;
                const u64 t = data[1] << 17;
                data[2] ^= data[0];
                data[3] ^= data[1];
                data[1] ^= data[2];
                data[0] ^= data[3];
                data[2] ^= t;
                data[3] = std::rotl(data[3], 45);
                return res;
            }
        } state_;
    };
    static_assert(std::uniform_random_bit_generator<xoshiro256>);

    using thread_rng_t = xoshiro256; ///< Type of the default thread-local PRNG.
    thread_rng_t& thread_rng(); ///< Get a thread-local PRNG seeded with the default random device.
    void reseed(); ///< Reseed the thread-local PRNG with the default random device.
    void reseed(thread_rng_t::result_type seed); ///< Reseed the thread-local PRNG with a given seed.

    /**
     * \brief Generate a random integer in an inclusive range with a uniform distribution.
     * \tparam T Type of the integer.
     * \tparam G Type of the RNG.
     * \param low Lower bound of the range.
     * \param high Upper bound of the range (inclusive).
     * \param rng The random number generator. Defaults to use the thread-local PRNG.
     */
    template <std::integral T, std::uniform_random_bit_generator G = thread_rng_t>
    T randint(const T low, const T high, G& rng = thread_rng())
    {
        // The standard missed these char types for some reason
        if constexpr (same_as_any_of<T, char, signed char, unsigned char>)
        {
            using int_type = std::conditional_t<std::is_signed_v<T>, i16, u16>;
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
     * \tparam G Type of the RNG.
     * \param low Lower bound of the range.
     * \param high Upper bound of the range (inclusive).
     * \param rng The random number generator. Defaults to use the thread-local PRNG.
     */
    template <std::floating_point T, std::uniform_random_bit_generator G = thread_rng_t>
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
     * \param rng The random number generator. Defaults to use the thread-local PRNG.
     * \return The end of the output region.
     */
    template <std::ranges::random_access_range R, std::ranges::random_access_range W, std::weakly_incrementable O,
        std::uniform_random_bit_generator G = thread_rng_t>
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
     * \param rng The random number generator. Defaults to use the thread-local PRNG.
     * \param alloc The allocator for intermediate allocations.
     * \return The end of the output region.
     * \see Efraimidis, P. S., & Spirakis, P. G. (2006). Weighted random sampling with a reservoir.
     * Information processing letters, 97(5), 181-185.
     */
    template <std::ranges::random_access_range R, std::ranges::random_access_range W, std::weakly_incrementable O,
        std::uniform_random_bit_generator G = thread_rng_t, allocator A = std::allocator<std::ranges::range_value_t<R>>>
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
