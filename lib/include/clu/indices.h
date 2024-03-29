#pragma once

#include <array>
#include <ranges>
#include <compare>

#include "assertion.h"
#include "iterator.h"

namespace clu
{
    template <std::size_t N>
    class indices_view : std::ranges::view_base
    {
        static_assert(N > 0, "at least one dimension");

    private:
        class iter_impl;

    public:
        using value_type = std::array<std::size_t, N>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = iterator_adapter<iter_impl>;
        using const_iterator = iterator;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<iterator>;

    private:
        struct sentinel
        {
        };

        class iter_impl
        {
        private:
            value_type extents_{};
            value_type current_{};

            template <std::size_t i>
            constexpr void increment_at() noexcept
            {
                if constexpr (i == 0)
                    ++current_[0];
                else if (++current_[i] == extents_[i])
                {
                    current_[i] = 0;
                    increment_at<i - 1>();
                }
            }

            template <std::size_t i>
            constexpr void decrement_at() noexcept
            {
                if constexpr (i == 0)
                    --current_[0];
                else if (current_[i]-- == 0)
                {
                    current_[i] += extents_[i];
                    decrement_at<i - 1>();
                }
            }

            template <std::size_t i>
            constexpr void add_at(const std::size_t amount) noexcept
            {
                if constexpr (i == 0)
                    current_[0] += amount;
                else if ((current_[i] += amount) >= extents_[i])
                {
                    this->add_at<i - 1>(amount / extents_[i]);
                    current_[i] %= extents_[i];
                }
            }

            template <std::size_t i>
            constexpr void subtract_at(const std::size_t amount) noexcept
            {
                if constexpr (i == 0)
                    current_[0] -= amount;
                else if (current_[i] < amount)
                {
                    const auto minus = amount - current_[i];
                    const auto multiple = (minus + extents_[i] - 1) / extents_[i];
                    current_[i] += multiple * extents_[i] - amount;
                    this->subtract_at<i - 1>(multiple);
                }
            }

            template <std::size_t i>
            constexpr std::ptrdiff_t distance_at(const value_type& other) const noexcept
            {
                if constexpr (i == 0)
                    return static_cast<std::ptrdiff_t>(current_[0]) - static_cast<std::ptrdiff_t>(other[0]);
                else
                    return static_cast<std::ptrdiff_t>(extents_[i]) * this->distance_at<i - 1>(other) +
                        static_cast<std::ptrdiff_t>(current_[i]) - static_cast<std::ptrdiff_t>(other[i]);
            }

        public:
            using reference = value_type;

            constexpr iter_impl() noexcept = default;
            constexpr explicit iter_impl(const value_type& extents, const value_type& current = {}) noexcept:
                extents_(extents), current_(current)
            {
            }

            constexpr reference operator*() const noexcept { return current_; }

            [[nodiscard]] constexpr bool operator==(const iter_impl& other) const noexcept
            {
                CLU_ASSERT(extents_ == other.extents_, "the iterators have different extents");
                return current_ == other.current_;
            }

            [[nodiscard]] constexpr bool operator==(sentinel) const noexcept { return current_[0] == extents_[0]; }

            [[nodiscard]] constexpr std::strong_ordering operator<=>(const iter_impl& other) const noexcept
            {
                CLU_ASSERT(extents_ == other.extents_, "the iterators have different extents");
                return current_ <=> other.current_;
            }

            constexpr iter_impl& operator++() noexcept
            {
                increment_at<N - 1>();
                return *this;
            }

            constexpr iter_impl& operator--() noexcept
            {
                decrement_at<N - 1>();
                return *this;
            }

            constexpr iter_impl& operator+=(const difference_type dist) noexcept
            {
                if (dist > 0)
                    add_at<N - 1>(static_cast<std::size_t>(dist));
                else if (dist < 0)
                    subtract_at<N - 1>(static_cast<std::size_t>(-dist));
                return *this;
            }

            [[nodiscard]] constexpr difference_type operator-(const iter_impl& other) const noexcept
            {
                CLU_ASSERT(extents_ == other.extents_, "the iterators have different extents");
                return this->distance_at<N - 1>(other.current_);
            }

            [[nodiscard]] constexpr difference_type operator-(sentinel) const noexcept
            {
                const value_type other{extents_[0]};
                return this->distance_at<N - 1>(other);
            }
        };

        value_type extents_{};

        constexpr iterator end_iter() const noexcept { return iter_impl(extents_, {extents_[0]}); }

    public:
        constexpr indices_view() noexcept = default;
        constexpr explicit indices_view(const value_type& extents) noexcept: extents_(extents) {}

        // clang-format off
        [[nodiscard]] constexpr iterator begin() const noexcept { return iter_impl(extents_); }
        [[nodiscard]] constexpr sentinel end() const noexcept { return {}; }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return iter_impl(extents_); }
        [[nodiscard]] constexpr sentinel cend() const noexcept { return {}; }
        [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end_iter()); }
        [[nodiscard]] constexpr reverse_iterator rend() const noexcept { return std::make_reverse_iterator(iter_impl(extents_));}
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }
        // clang-format on

        [[nodiscard]] constexpr value_type front() const noexcept { return {}; }

        [[nodiscard]] constexpr value_type back() const noexcept
        {
            return [&]<std::size_t... i>(std::index_sequence<i...>)->value_type { return {(extents_[i] - 1)...}; }
            (std::make_index_sequence<N>{});
        }

        [[nodiscard]] constexpr value_type operator[](const size_type i) const noexcept { return begin()[i]; }

        [[nodiscard]] constexpr size_type size() const noexcept
        {
            return [this]<size_t... i>(std::index_sequence<i...>) { return (extents_[i] * ... * 1); }
            (std::make_index_sequence<N>{});
        }
        [[nodiscard]] constexpr size_type empty() const noexcept { return extents_ == value_type{}; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return extents_ != value_type{}; }

        [[nodiscard]] constexpr static size_type dimension() noexcept { return N; }
        [[nodiscard]] constexpr const value_type& extents() const noexcept { return extents_; }
    };

    template <std::size_t N>
    indices_view(std::array<std::size_t, N>) -> indices_view<N>;

    template <std::convertible_to<std::size_t>... IdxTypes>
    [[nodiscard]] constexpr auto indices(const IdxTypes... extents) noexcept
    {
        return indices_view<sizeof...(IdxTypes)>({static_cast<std::size_t>(extents)...});
    }
} // namespace clu
