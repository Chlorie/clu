#pragma once

#include <array>

namespace clu
{
    namespace detail
    {
        template <size_t N>
        class indices_t final
        {
            static_assert(N > 0, "At least one index");

        public:
            using value_type = std::array<size_t, N>;

        private:
            value_type extents_{};

        public:
            struct sentinel final {};

            class iterator final
            {
            public:
                using difference_type = ptrdiff_t;
                using value_type = value_type;
                using pointer = const value_type*;
                using reference = const value_type&;
                using iterator_category = std::forward_iterator_tag;

            private:
                value_type data_{};
                value_type extents_{};

                template <size_t I>
                constexpr void increment_at()
                {
                    if constexpr (I == 0)
                        ++data_[0];
                    else
                    {
                        if (++data_[I] == extents_[I])
                        {
                            data_[I] = 0;
                            increment_at<I - 1>();
                        }
                    }
                }

            public:
                constexpr iterator() = default;
                explicit constexpr iterator(const value_type& extents): extents_(extents) {}

                constexpr reference operator*() const { return data_; }
                constexpr pointer operator->() const { return &data_; }

                constexpr iterator& operator++()
                {
                    increment_at<N - 1>();
                    return *this;
                }

                constexpr iterator operator++(int)
                {
                    const iterator result = *this;
                    operator++();
                    return result;
                }

                friend constexpr bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.data_ == rhs.data_; }
                friend constexpr bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.data_ != rhs.data_; }
                friend constexpr bool operator==(const iterator& it, sentinel) { return it.data_[0] == it.extents_[0]; }
                friend constexpr bool operator==(sentinel, const iterator& it) { return it.data_[0] == it.extents_[0]; }
                friend constexpr bool operator!=(const iterator& it, sentinel) { return it.data_[0] != it.extents_[0]; }
                friend constexpr bool operator!=(sentinel, const iterator& it) { return it.data_[0] != it.extents_[0]; }
            };

            using const_iterator = iterator;

            explicit constexpr indices_t(const value_type& extents): extents_(extents) {}

            constexpr auto begin() const { return iterator{ extents_ }; }
            constexpr auto cbegin() const { return iterator{ extents_ }; }
            constexpr auto end() const { return sentinel{}; }
            constexpr auto cend() const { return sentinel{}; }
        };

        template <>
        class indices_t<1> final // Only one index
        {
        public:
            using value_type = size_t;

        private:
            size_t extent_ = 0;

        public:
            class iterator final
            {
            public:
                using difference_type = ptrdiff_t;
                using value_type = size_t;
                using pointer = const value_type*;
                using reference = value_type;
                using iterator_category = std::forward_iterator_tag;

            private:
                size_t data_;

            public:
                explicit constexpr iterator(const size_t data = 0): data_(data) {}

                constexpr reference operator*() const { return data_; }
                constexpr pointer operator->() const { return &data_; }

                constexpr iterator& operator++()
                {
                    ++data_;
                    return *this;
                }

                constexpr iterator operator++(int)
                {
                    const iterator result = *this;
                    operator++();
                    return result;
                }

                friend constexpr bool operator==(const iterator lhs, const iterator rhs) { return lhs.data_ == rhs.data_; }
                friend constexpr bool operator!=(const iterator lhs, const iterator rhs) { return lhs.data_ != rhs.data_; }
            };

            using const_iterator = iterator;

            explicit constexpr indices_t(const size_t extent): extent_(extent) {}

            constexpr auto begin() const { return iterator{}; }
            constexpr auto cbegin() const { return iterator{}; }
            constexpr auto end() const { return iterator{ extent_ }; }
            constexpr auto cend() const { return iterator{ extent_ }; }
        };
    }

    template <typename... Ints>
    constexpr auto indices(const Ints ... extents) { return detail::indices_t<sizeof...(Ints)>({ static_cast<size_t>(extents)... }); }
}
