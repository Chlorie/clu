#pragma once

#include <array>

#include "integer_literals.h"
#include "static_for.h"

namespace clu
{
    template <size_t N>
    class big_uint
    {
        static_assert(N > 0, "At least one unit");
    public:
        using array_type = std::array<u64, N>;

    private:
        array_type data_{}; // Little endian storage

    public:
        constexpr big_uint() = default;
        constexpr ~big_uint() = default;
        constexpr big_uint(const big_uint&) = default;
        constexpr big_uint(big_uint&&) = default;
        constexpr big_uint& operator=(const big_uint&) = default;
        constexpr big_uint& operator=(big_uint&&) = default;

        constexpr explicit big_uint(const array_type& arr) noexcept: data_(arr) {}
        constexpr explicit(false) big_uint(const u64 other = 0) noexcept: data_{ other } {}

        template <size_t M>
        constexpr explicit(M > N) big_uint(const big_uint<M>& other) noexcept
        {
            clu::static_for<0, std::min(M, N)>([&](const size_t i) { data_[i] = other.data_[i]; });
        }

        [[nodiscard]] constexpr array_type& data() noexcept { return data_; }
        [[nodiscard]] constexpr const array_type& data() const noexcept { return data_; }
        [[nodiscard]] constexpr u64& operator[](const size_t index) noexcept { return data_[index]; }
        [[nodiscard]] constexpr const u64& operator[](const size_t index) const noexcept { return data_[index]; }
        [[nodiscard]] constexpr explicit operator u64() const noexcept { return data_[0]; }
    };
}
