#pragma once

#include <iterator>

namespace clu
{
    template <typename Rng>
    class enumerate_t final
    {
    private:
        using Iter = decltype(std::begin(std::declval<Rng>()));
        using Sent = decltype(std::end(std::declval<Rng>()));
        using Ref = decltype(*std::declval<const Iter>());

        static_assert(std::is_base_of_v<std::input_iterator_tag, typename Iter::iterator_category>,
            "Iterators of the range should be at least input iterators");

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

            reference operator*() const { return { index_, *it_ }; }

            iterator& operator++()
            {
                ++index_;
                ++it_;
                return *this;
            }

            iterator operator++(int)
            {
                iterator res = *this;
                operator++();
                return res;
            }

            friend constexpr bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.it_ == rhs.it_; }
            friend constexpr bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.it_ != rhs.it_; }
            friend constexpr bool operator==(const iterator& it, const sentinel& sent) { return it.it_ == sent.sent; }
            friend constexpr bool operator==(const sentinel& sent, const iterator& it) { return it.it_ == sent.sent; }
            friend constexpr bool operator!=(const iterator& it, const sentinel& sent) { return it.it_ != sent.sent; }
            friend constexpr bool operator!=(const sentinel& sent, const iterator& it) { return it.it_ != sent.sent; }
        };

        using const_iterator = iterator;

    private:
        Rng range_;

    public:
        explicit constexpr enumerate_t(Rng&& range): range_(static_cast<Rng&&>(range)) {}

        auto begin() { return iterator(std::begin(range_)); }
        auto end() { return sentinel{ std::end(range_) }; }
        auto cbegin() const { return iterator(std::cbegin(range_)); }
        auto cend() const { return sentinel{ std::cend(range_) }; }
        auto begin() const { return cbegin(); }
        auto end() const { return cend(); }
    };

    template <typename Rng>
    auto enumerate(Rng&& range) { return enumerate_t<Rng>(std::forward<Rng>(range)); }
}
