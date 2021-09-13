#pragma once

#include <iterator>
#include <memory>
#include <stdexcept>
#include <ranges>
#include <algorithm>
#include <compare>

#include "scope.h"
#include "integer_literals.h"

namespace clu
{
    struct list_init_t {};
    inline constexpr list_init_t list_init{};
    
    template <typename T, size_t N>
    class static_vector
    {
    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<T*>;
        using const_reverse_iterator = std::reverse_iterator<const T*>;

    private:
        static constexpr bool nothrow_swap =
            std::is_nothrow_swappable_v<T> &&
            std::is_nothrow_move_constructible_v<T> &&
            std::is_nothrow_destructible_v<T>;

        static constexpr bool nothrow_move_assign =
            std::is_nothrow_move_constructible_v<T> &&
            std::is_nothrow_destructible_v<T>;

        alignas(T) unsigned char storage_[N * sizeof(T)]{};
        size_t size_ = 0;

        [[noreturn]] static void size_exceed_capacity() { throw std::out_of_range("specified size exceeds capacity"); }
        [[noreturn]] static void add_too_many() { throw std::out_of_range("trying to add too many elements"); }

        template <std::input_iterator It, std::sentinel_for<It> Se>
        void assign_range(It first, const Se last)
        {
            if constexpr (std::forward_iterator<It>)
            {
                const auto dist = static_cast<size_t>(std::ranges::distance(first, last));
                if (dist > N) size_exceed_capacity();
                // TODO: remove this MSVC STL issue workaround
                const auto common = std::views::common(std::ranges::subrange(first, last));
                std::uninitialized_copy(common.begin(), common.end(), data());
                // std::ranges::uninitialized_copy(first, last, data(), data() + dist);
                size_ = dist;
            }
            else
            {
                scope_fail guard([this] { clear(); });
                for (; first != last; ++first, ++size_)
                {
                    if (size_ == N) size_exceed_capacity();
                    new(data() + size_) T(*first);
                }
            }
        }

        size_t checked_pos(const size_t pos) const
        {
            if (pos >= size_)
                throw std::out_of_range("static_vector index out of range");
            return pos;
        }

        iterator strip_const(const const_iterator iter) { return begin() + (iter - cbegin()); }

        iterator move_one_place(const const_iterator pos)
        {
            if (size_ == N) add_too_many();
            new(data() + size_) T();
            size_++;
            const iterator iter = strip_const(pos);
            std::move_backward(iter, end() - 1, end());
            return iter;
        }

        iterator move_n_place(const const_iterator pos, const size_t count)
        {
            if (size_ + count > N) add_too_many();
            std::uninitialized_value_construct_n(data() + size_, count);
            size_ += count;
            const iterator iter = strip_const(pos);
            std::move_backward(iter, end() - count, end());
            return iter;
        }

        void shrink_to_size(const size_t count)
        {
            std::destroy(begin() + count, end());
            size_ = count;
        }

    public:
        constexpr static_vector() noexcept = default;

        static_vector(const size_t count, const T& value)
        {
            if (count > N) size_exceed_capacity();
            std::uninitialized_fill_n(data(), count, value);
            size_ = count;
        }

        explicit static_vector(const size_t count)
        {
            if (count > N) size_exceed_capacity();
            std::uninitialized_value_construct_n(data(), count);
            size_ = count;
        }

        template <std::input_iterator It, std::sentinel_for<It> Se>
        static_vector(const It first, const Se last) { assign_range(first, last); }

        template <std::ranges::input_range Rng>
        explicit static_vector(Rng&& range): static_vector(range.begin(), range.end()) {}

        static_vector(const static_vector& other)
        {
            std::uninitialized_copy(other.begin(), other.end(), data());
            size_ = other.size_;
        }

        static_vector(static_vector&& other) noexcept(std::is_nothrow_move_assignable_v<T>)
        {
            std::uninitialized_move(other.begin(), other.end(), data());
            size_ = other.size_;
        }

        static_vector(const std::initializer_list<T> init): static_vector(init.begin(), init.end()) {}

        template <typename... Ts> requires (std::convertible_to<Ts&&, T> && ...)
        explicit static_vector(list_init_t, Ts&&... init)
        {
            if (sizeof...(init) > N) size_exceed_capacity();
            scope_fail guard([this] { clear(); });
            const auto construct_one = [this]<typename U>(U&& value)
            {
                new(data() + size_) T(std::forward<U>(value));
                size_++;
            };
            (construct_one(init), ...);
        }

        ~static_vector() noexcept { clear(); }

        static_vector& operator=(const static_vector& other)
        {
            clear();
            std::uninitialized_copy(other.begin(), other.end(), data());
            size_ = other.size_;
            return *this;
        }

        static_vector& operator=(static_vector&& other) noexcept(nothrow_move_assign)
        {
            clear();
            std::uninitialized_move(other.begin(), other.end(), data());
            size_ = other.size_;
            return *this;
        }

        static_vector& operator=(const std::initializer_list<T> init)
        {
            if (init.size() > N) throw std::out_of_range("initializer list too large for this static_vector");
            clear();
            std::uninitialized_copy(init.begin(), init.end(), data());
            size_ = init.size();
            return *this;
        }

        void assign(const size_t count, const T& value)
        {
            if (count > N) size_exceed_capacity();
            clear();
            std::uninitialized_fill_n(data(), count, value);
            size_ = count;
        }

        template <std::input_iterator It, std::sentinel_for<It> Se>
        void assign(const It first, const Se last)
        {
            clear();
            assign_range(first, last);
        }

        template <std::ranges::input_range Rng>
        void assign(Rng&& range) { assign(range.begin(), range.end()); }

        void assign(const std::initializer_list<T> init) { *this = init; }

        [[nodiscard]] T& at(const size_t pos) { return data()[checked_pos(pos)]; }
        [[nodiscard]] const T& at(const size_t pos) const { return data()[checked_pos(pos)]; }
        [[nodiscard]] T& operator[](const size_t pos) noexcept { return data()[pos]; }
        [[nodiscard]] const T& operator[](const size_t pos) const noexcept { return data()[pos]; }
        [[nodiscard]] T& front() noexcept { return *data(); }
        [[nodiscard]] const T& front() const noexcept { return *data(); }
        [[nodiscard]] T& back() noexcept { return data()[size_ - 1]; }
        [[nodiscard]] const T& back() const noexcept { return data()[size_ - 1]; }
        [[nodiscard]] T* data() noexcept { return std::launder(reinterpret_cast<T*>(storage_)); }
        [[nodiscard]] const T* data() const noexcept { return std::launder(reinterpret_cast<const T*>(storage_)); }

        [[nodiscard]] iterator begin() noexcept { return data(); }
        [[nodiscard]] const_iterator begin() const noexcept { return data(); }
        [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }
        [[nodiscard]] iterator end() noexcept { return data() + size_; }
        [[nodiscard]] const_iterator end() const noexcept { return data() + size_; }
        [[nodiscard]] const_iterator cend() const noexcept { return data() + size_; }
        [[nodiscard]] reverse_iterator rbegin() noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] const_reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] const_reverse_iterator crbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] reverse_iterator rend() noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] const_reverse_iterator rend() const noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] const_reverse_iterator crend() const noexcept { return std::make_reverse_iterator(begin()); }

        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
        [[nodiscard]] constexpr size_t size() const noexcept { return size_; }
        [[nodiscard]] constexpr size_t max_size() const noexcept { return N; }
        [[nodiscard]] constexpr size_t capacity() const noexcept { return N; }

        void clear() noexcept
        {
            std::destroy_n(data(), size_);
            size_ = 0;
        }

        iterator insert(const const_iterator pos, const T& value)
        {
            const iterator iter = move_one_place(pos);
            *iter = value;
            return iter;
        }

        iterator insert(const const_iterator pos, T&& value)
        {
            const iterator iter = move_one_place(pos);
            *iter = std::move(value);
            return iter;
        }

        iterator insert(const const_iterator pos, const size_t count, const T& value)
        {
            const iterator iter = move_n_place(pos, count);
            std::fill_n(iter, count, value);
            return iter;
        }

        template <std::input_iterator It, std::sentinel_for<It> Se>
        iterator insert(const const_iterator pos, It first, const Se last)
        {
            if constexpr (std::forward_iterator<It>)
            {
                const auto count = std::ranges::distance(first, last);
                const iterator iter = move_n_place(pos, count);
                std::ranges::copy(first, last, iter);
                return iter;
            }
            else // Why are you even doing this?
            {
                const iterator result = strip_const(pos);
                for (iterator iter = result; first != last; ++first)
                    iter = insert(iter, *first) + 1;
                return result;
            }
        }

        template <std::ranges::input_range Rng>
        iterator insert(const const_iterator pos, Rng&& range) { return insert(pos, range.begin(), range.end()); }

        iterator insert(const const_iterator pos, const std::initializer_list<T> init) { return insert(pos, init.begin(), init.end()); }

        template <typename... Args> requires std::constructible_from<T, Args&&...>
        iterator emplace(const const_iterator pos, Args&&... args)
        {
            const iterator iter = move_one_place(pos);
            iter->~T();
            new(std::addressof(*iter)) T(std::forward<Args>(args)...);
            return iter;
        }

        iterator erase(const const_iterator pos)
        {
            const iterator iter = strip_const(pos);
            std::move(iter + 1, end(), iter);
            size_--;
            end()->~T();
            return iter;
        }

        iterator erase(const const_iterator first, const const_iterator last)
        {
            const iterator ifirst = strip_const(first), ilast = strip_const(last);
            std::move(ilast, end(), ifirst);
            const auto dist = ilast - ifirst;
            size_ -= dist;
            std::destroy(end(), end() + dist);
            return ifirst;
        }

        void push_back(const T& value)
        {
            if (size_ == N) add_too_many();
            new(end()) T(value);
            size_++;
        }

        void push_back(T&& value)
        {
            if (size_ == N) add_too_many();
            new(end()) T(std::move(value));
            size_++;
        }

        template <typename... Args> requires std::constructible_from<T, Args&&...>
        T& emplace_back(Args&&... args)
        {
            if (size_ == N) add_too_many();
            T* ptr = new(end()) T(std::forward<Args>(args)...);
            size_++;
            return *ptr;
        }

        void pop_back()
        {
            back().~T();
            size_--;
        }

        void resize(const size_t count)
        {
            if (count <= size_)
                std::destroy(begin() + count, end());
            else
                std::uninitialized_value_construct(end(), begin() + count);
            size_ = count;
        }

        void resize(const size_t count, const T& value)
        {
            if (count <= size_)
                std::destroy(begin() + count, end());
            else
                std::uninitialized_fill(end(), begin() + count, value);
            size_ = count;
        }

        void swap(static_vector& other) noexcept(nothrow_swap)
        {
            if (other.size_ < size_)
                other.swap(*this);
            else
            {
                std::swap_ranges(begin(), end(), other.begin());
                std::uninitialized_move(other.begin() + size_, other.end(), end());
                std::destroy(other.begin() + size_, other.end());
                std::swap(size_, other.size_);
            }
        }

        [[nodiscard]] bool operator==(const static_vector& other) const { return std::ranges::equal(*this, other); }
        [[nodiscard]] auto operator<=>(const static_vector& other) const { return std::ranges::lexicographical_compare(*this, other); }

        friend void swap(static_vector& lhs, static_vector& rhs) noexcept(nothrow_swap) { lhs.swap(rhs); }
    };

    template <typename T, size_t N, std::equality_comparable_with<T> U>
    size_t erase(static_vector<T, N>& vec, const U& value)
    {
        const auto it = std::ranges::remove(vec, value);
        const auto dist = vec.end() - it;
        vec.erase(it, vec.end());
        return static_cast<size_t>(dist);
    }

    template <typename T, size_t N, std::indirect_unary_predicate<typename static_vector<T, N>::iterator> Pr>
    size_t erase_if(static_vector<T, N>& vec, const Pr pred)
    {
        const auto it = std::ranges::remove_if(vec, pred);
        const auto dist = vec.end() - it;
        vec.erase(it, vec.end());
        return static_cast<size_t>(dist);
    }
}
