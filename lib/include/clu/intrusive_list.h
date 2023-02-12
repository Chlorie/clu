#pragma once

#include <utility>

#include "intrusive_list.h"
#include "iterator.h"
#include "macros.h"

namespace clu
{
    template <typename T>
    class intrusive_list;

    template <typename T>
    class intrusive_list_element_base // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
    private:
        friend intrusive_list<T>;
        union
        {
            T* tail_ = nullptr;
            T* prev_;
        };
        union
        {
            intrusive_list_element_base* next_ = nullptr;
            intrusive_list_element_base* head_; // head_ may be the sentinel node
        };
    };

    template <typename T>
    class intrusive_list
    {
        static_assert(std::is_base_of_v<intrusive_list_element_base<T>, T>,
            "intrusive_list element type T must derive from CRTP base class intrusive_list_element_base<T>");

    public:
        using value_type = T;
        using size_type = size_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;

    private:
        using node_base_type = intrusive_list_element_base<T>;

        template <bool IsConst>
        class iters_impl
        {
        public:
            constexpr iters_impl() = default;
            // clang-format off
            constexpr explicit(false) iters_impl(
                const iters_impl<false>& iter) noexcept requires(IsConst): ptr_(iter.ptr_) {}
            // clang-format on
            constexpr iters_impl(const iters_impl&) noexcept = default;
            constexpr iters_impl& operator=(const iters_impl&) noexcept = default;
            constexpr friend bool operator==(iters_impl, iters_impl) noexcept = default;
            constexpr auto& operator*() const noexcept { return *static_cast<qualified_ptr>(ptr_); }

            constexpr iters_impl& operator++() noexcept
            {
                ptr_ = ptr_->next_;
                return *this;
            }

            constexpr iters_impl& operator--() noexcept
            {
                ptr_ = base_ptr(ptr_->prev_);
                return *this;
            }

        private:
            friend intrusive_list;
            friend iters_impl<true>;
            using qualified_ptr = conditional_t<IsConst, const T*, T*>;
            using qualified_base_ptr = conditional_t<IsConst, const node_base_type*, node_base_type*>;

            qualified_base_ptr ptr_ = nullptr;

            constexpr explicit iters_impl(qualified_base_ptr ptr) noexcept: ptr_(ptr) {}
        };

        using iter_impl = iters_impl<false>;
        using citer_impl = iters_impl<true>;

    public:
        using iterator = iterator_adapter<iters_impl<false>>;
        using const_iterator = iterator_adapter<iters_impl<true>>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        CLU_NON_COPYABLE_TYPE(intrusive_list);

        constexpr intrusive_list() noexcept { reset_sentinel(); }

        constexpr intrusive_list(intrusive_list&& other) noexcept
        {
            sentinel_ = other.sentinel_;
            other.reset_sentinel();
            fix_tail();
        }

        constexpr ~intrusive_list() noexcept { clear(); }

        constexpr intrusive_list& operator=(intrusive_list&& other) noexcept
        {
            if (&other == this)
                return *this;
            clear();
            sentinel_ = other.sentinel_;
            other.reset_sentinel();
            fix_tail();
            return *this;
        }

        constexpr void swap(intrusive_list& other) noexcept
        {
            std::swap(sentinel_, other.sentinel_);
            fix_tail();
            other.fix_tail();
        }

        constexpr friend void swap(intrusive_list& lhs, intrusive_list& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] constexpr T& front() noexcept { return *elem_head(); }
        [[nodiscard]] constexpr const T& front() const noexcept { return *elem_head(); }
        [[nodiscard]] constexpr T& back() noexcept { return *sentinel_.tail_; }
        [[nodiscard]] constexpr const T& back() const noexcept { return *sentinel_.tail_; }

        // clang-format off
        [[nodiscard]] constexpr iterator begin() noexcept { return iterator(iter_impl(sentinel_.head_)); }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(citer_impl(sentinel_.head_)); }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return const_iterator(citer_impl(sentinel_.head_)); }
        [[nodiscard]] constexpr iterator end() noexcept { return iterator(iter_impl(&sentinel_)); }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(citer_impl(&sentinel_)); }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return const_iterator(citer_impl(&sentinel_)); }
        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr reverse_iterator rend() noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return std::make_reverse_iterator(begin()); }
        // clang-format on

        [[nodiscard]] constexpr static iterator get_iterator(T& element) noexcept
        {
            return iterator(iter_impl(base_ptr(std::addressof(element))));
        }
        [[nodiscard]] constexpr static const_iterator get_iterator(const T& element) noexcept
        {
            return const_iterator(citer_impl(base_ptr(std::addressof(element))));
        }

        [[nodiscard]] constexpr bool empty() const noexcept { return !sentinel_.tail_; }

        constexpr void clear() noexcept
        {
            if (!sentinel_.head_)
                return;
            node_base_type* ptr = sentinel_.head_;
            while (ptr != &sentinel_)
            {
                ptr->prev_ = nullptr;
                ptr = std::exchange(ptr->next_, nullptr);
            }
            reset_sentinel();
        }

        constexpr iterator insert(const const_iterator pos, T& element) noexcept
        {
            if (pos == begin())
            {
                this->push_front(element);
                return begin();
            }
            if (pos == end())
            {
                this->push_back(element);
                return get_iterator(element);
            }
            T* ptr = std::addressof(element);
            auto* posptr = const_cast<node_base_type*>(pos.ptr_);
            T* prev = posptr->prev_;
            base_ptr(ptr)->prev_ = prev;
            base_ptr(ptr)->next_ = posptr;
            base_ptr(prev)->next_ = base_ptr(ptr);
            posptr->prev_ = ptr;
            return get_iterator(element);
        }

        constexpr void push_front(T& element) noexcept
        {
            T* ptr = std::addressof(element);
            base_ptr(ptr)->next_ = sentinel_.head_;
            sentinel_.head_->prev_ = ptr;
            sentinel_.head_ = base_ptr(ptr);
        }

        constexpr void push_back(T& element) noexcept
        {
            T* ptr = std::addressof(element);
            base_ptr(ptr)->prev_ = sentinel_.tail_;
            base_ptr(ptr)->next_ = &sentinel_;
            (sentinel_.tail_ ? base_ptr(sentinel_.tail_)->next_ : sentinel_.head_) = base_ptr(ptr);
            sentinel_.tail_ = ptr;
        }

        constexpr iterator erase(const const_iterator pos) noexcept
        {
            if (pos == begin())
            {
                pop_front();
                return begin();
            }
            if (pos.ptr_ == base_ptr(sentinel_.tail_))
            {
                pop_back();
                return end();
            }
            auto* posptr = const_cast<node_base_type*>(pos.ptr_);
            base_ptr(posptr->prev_)->next_ = posptr->next_;
            posptr->next_->prev_ = posptr->prev_;
            const iterator res(iter_impl(posptr->next_));
            *posptr = {};
            return res;
        }

        constexpr void pop_front() noexcept
        {
            sentinel_.head_ = std::exchange(sentinel_.head_->next_, nullptr);
            (sentinel_.head_ == &sentinel_ ? sentinel_.head_->prev_ : sentinel_.tail_) = nullptr;
        }

        constexpr void pop_back() noexcept
        {
            sentinel_.tail_ = std::exchange(base_ptr(sentinel_.tail_)->prev_, nullptr);
            (sentinel_.tail_ ? base_ptr(sentinel_.tail_)->next_ : sentinel_.head_) = &sentinel_;
        }

    private:
        node_base_type sentinel_;

        constexpr T* elem_head() noexcept { return elem_ptr(sentinel_.next_); }
        constexpr const T* elem_head() const noexcept { return elem_ptr(sentinel_.next_); }

        constexpr void fix_tail() noexcept
        {
            if (auto* last = base_ptr(sentinel_.tail_))
                last->next_ = &sentinel_;
        }
        constexpr void reset_sentinel() noexcept
        {
            sentinel_.tail_ = nullptr;
            sentinel_.head_ = &sentinel_;
        }

        CLU_GCC_WNO_OLD_STYLE_CAST
        constexpr static node_base_type* base_ptr(T* ptr) noexcept
        {
            // ReSharper disable once CppCStyleCast
            return (intrusive_list_element_base<T>*)ptr;
        }
        constexpr static const node_base_type* base_ptr(const T* ptr) noexcept
        {
            // ReSharper disable once CppCStyleCast
            return (const intrusive_list_element_base<T>*)ptr;
        }

        constexpr static T* elem_ptr(node_base_type* ptr)
        {
            // ReSharper disable once CppCStyleCast
            return (T*)ptr;
        }
        constexpr static const T* elem_ptr(const node_base_type* ptr)
        {
            // ReSharper disable once CppCStyleCast
            return (const T*)ptr;
        }
        CLU_GCC_RESTORE_WARNING
    };
} // namespace clu
