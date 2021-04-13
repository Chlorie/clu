#pragma once

#include <iterator>
#include <ranges>

namespace clu
{
    namespace detail
    {
        template <typename Rng>
        class enumerate_t final
        {
        private:
            using Iter = std::ranges::iterator_t<Rng>;
            using Sent = std::ranges::sentinel_t<Rng>;
            using Ref = std::ranges::range_reference_t<Rng>;

        public:
            struct sentinel final
            {
                Sent sent;
            };

            class iterator final
            {
            public:
                using difference_type = ptrdiff_t;
                using value_type = std::pair<size_t, Ref>;
                using pointer = value_type*;
                using reference = value_type;
                using iterator_category = std::input_iterator_tag;

            private:
                size_t index_ = 0;
                Iter it_;

            public:
                constexpr iterator() = default;
                explicit constexpr iterator(const Iter iter): it_(iter) {}

                [[nodiscard]] constexpr reference operator*() { return { index_, *it_ }; }

                constexpr iterator& operator++()
                {
                    ++index_;
                    ++it_;
                    return *this;
                }

                constexpr iterator operator++(int)
                {
                    iterator res = *this;
                    operator++();
                    return res;
                }

                [[nodiscard]] friend constexpr bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.it_ == rhs.it_; }
                [[nodiscard]] friend constexpr bool operator==(const iterator& it, const sentinel& sent) { return it.it_ == sent.sent; }
            };

            using const_iterator = iterator;

        private:
            Rng range_; // Take ownership if rvalue, take reference if lvalue

        public:
            explicit constexpr enumerate_t(Rng&& range): range_(static_cast<Rng&&>(range)) {}

            [[nodiscard]] constexpr auto begin() { return iterator(std::begin(range_)); }
            [[nodiscard]] constexpr auto end() { return sentinel{ std::end(range_) }; }
            [[nodiscard]] constexpr auto cbegin() const { return iterator(std::cbegin(range_)); }
            [[nodiscard]] constexpr auto cend() const { return sentinel{ std::cend(range_) }; }
            [[nodiscard]] constexpr auto begin() const { return cbegin(); }
            [[nodiscard]] constexpr auto end() const { return cend(); }
        };
    }

    template <std::ranges::input_range Rng>
    [[nodiscard]] constexpr auto enumerate(Rng&& range) { return detail::enumerate_t<Rng>(std::forward<Rng>(range)); }
}
