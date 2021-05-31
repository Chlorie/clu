#pragma once

#include <memory>
#include <memory_resource>

#include "take.h"
#include "iterator.h"
#include "assertion.h"

namespace clu
{
    template <typename T, typename Alloc = std::allocator<T>>
    class forest
    {
        static_assert(std::is_same_v<typename std::allocator_traits<Alloc>::value_type, T>,
            "value_type of the allocator type Alloc should be the same as T for clu::forest<T, Alloc>");
        static_assert(std::is_same_v<typename std::allocator_traits<Alloc>::pointer, T*>,
            "allocators that allocates fancy pointers are not supported");
    private:
        struct node;
        struct node_base;
        class iter_base;
        class const_iter_base;
        class iter_impl;
        class const_iter_impl;
        class full_iter_impl;
        class const_full_iter_impl;
        class child_iter_impl;
        class const_child_iter_impl;

    public:
        using value_type = T;
        using allocator_type = Alloc;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;

        using iterator = iterator_adapter<iter_impl>;
        using const_iterator = iterator_adapter<const_iter_impl>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using preorder_iterator = iterator;
        using const_preorder_iterator = const_iterator;
        using preorder_range = std::ranges::subrange<preorder_iterator>;
        using const_preorder_range = std::ranges::subrange<const_preorder_iterator>;
        using full_order_iterator = iterator_adapter<full_iter_impl>;
        using const_full_order_iterator = iterator_adapter<const_full_iter_impl>;
        using full_order_range = std::ranges::subrange<full_order_iterator>;
        using const_full_order_range = std::ranges::subrange<const_full_order_iterator>;
        using child_iterator = iterator_adapter<child_iter_impl>;
        using const_child_iterator = iterator_adapter<const_child_iter_impl>;
        using child_range = std::ranges::subrange<child_iterator>;
        using const_child_range = std::ranges::subrange<const_child_iterator>;

    private:
        struct node_base
        {
            node* child{};
            node_base* parent{};

            constexpr node* derived() noexcept { return static_cast<node*>(this); }
            constexpr const node* derived() const noexcept { return static_cast<const node*>(this); }

            constexpr void fix_children_parents() noexcept
            {
                if (!child) return;
                node* ptr = child;
                do
                {
                    ptr->parent = this;
                    ptr = ptr->next;
                } while (ptr != child);
            }
        };

        struct node : node_base
        {
            T value;
            node *prev{}, *next{};

            template <typename... Ts> requires std::constructible_from<T, Ts&&...>
            constexpr explicit node(Ts&&... args): value(std::forward<Ts>(args)...) {}

            constexpr bool is_last_sibling() const noexcept { return this->parent->child == next; }
            constexpr bool is_first_sibling() const noexcept { return this->parent->child == this; }
        };

        class iter_base
        {
            friend forest;
        protected:
            node_base* ptr_{};

        public:
            constexpr iter_base() = default;
            constexpr explicit iter_base(node_base* ptr) noexcept: ptr_(ptr) {}
            constexpr explicit(false) operator const_iter_base() const noexcept { return const_iter_base(ptr_); }

            [[nodiscard]] constexpr T& operator*() const noexcept { return ptr_->derived()->value; }
            [[nodiscard]] constexpr friend bool operator==(iter_base, iter_base) noexcept = default;
            [[nodiscard]] constexpr iterator preorder() const noexcept { return iterator(ptr_); }

            [[nodiscard]] constexpr bool is_leaf() const noexcept { return !ptr_->child; }
            [[nodiscard]] constexpr bool is_root() const noexcept { return !ptr_->parent->parent; }
            [[nodiscard]] constexpr iterator parent() const noexcept { return iterator(ptr_->parent); }
            [[nodiscard]] constexpr iterator next_sibling() const noexcept { return iterator(ptr_->derived()->next); }
            [[nodiscard]] constexpr iterator prev_sibling() const noexcept { return iterator(ptr_->derived()->prev); }

            [[nodiscard]] constexpr auto children() const noexcept
            {
                node_base* first = ptr_->child;
                return child_range{ child_iterator(first, !first), child_iterator(first, true) };
            }
        };

        class const_iter_base
        {
            friend forest;
        protected:
            const node_base* ptr_{};

        public:
            constexpr const_iter_base() = default;
            constexpr explicit const_iter_base(const node_base* ptr): ptr_(ptr) {}
            constexpr explicit operator iter_base() const noexcept { return iter_base(const_cast<node_base*>(ptr_)); }

            [[nodiscard]] constexpr const T& operator*() const noexcept { return ptr_->derived()->value; }
            [[nodiscard]] constexpr friend bool operator==(const_iter_base, const_iter_base) noexcept = default;
            [[nodiscard]] constexpr const_iterator preorder() const noexcept { return const_iterator(ptr_); }

            [[nodiscard]] constexpr bool is_leaf() const noexcept { return !ptr_->child; }
            [[nodiscard]] constexpr bool is_root() const noexcept { return !ptr_->parent->parent; }
            [[nodiscard]] constexpr const_iterator parent() const noexcept { return const_iterator(ptr_->parent); }
            [[nodiscard]] constexpr const_iterator next_sibling() const noexcept { return const_iterator(ptr_->derived()->next); }
            [[nodiscard]] constexpr const_iterator prev_sibling() const noexcept { return const_iterator(ptr_->derived()->prev); }

            [[nodiscard]] constexpr auto children() const noexcept
            {
                const node_base* first = ptr_->child;
                return const_child_range{ const_child_iterator(first, !first), const_child_iterator(first, true) };
            }
        };

        using iter_data = std::pair<node_base*, bool>;

        constexpr static iter_data full_next(const node_base* ptr, const bool revisit) noexcept
        {
            if (revisit)
            {
                if (const auto nptr = ptr->derived();
                    nptr->is_last_sibling())
                    return { nptr->parent, true };
                else
                    return { nptr->next, false };
            }
            else
            {
                if (ptr->child)
                    return { ptr->child, false };
                else
                    return { const_cast<node_base*>(ptr), true };
            }
        }

        constexpr static iter_data full_prev(const node_base* ptr, const bool revisit) noexcept
        {
            if (revisit)
            {
                if (ptr->child)
                    return { ptr->child, true };
                else
                    return { const_cast<node_base*>(ptr), false };
            }
            else
            {
                if (const auto nptr = ptr->derived();
                    nptr->is_first_sibling())
                    return { nptr->parent, false };
                else
                    return { nptr->prev, true };
            }
        }

        constexpr static node_base* pre_next(const node_base* ptr)
        {
            CLU_ASSERT(ptr->parent, "trying to increment end iterator");
            auto [res, revisit] = full_next(ptr, false);
            if (!revisit || !res->parent) return res;
            while (true)
            {
                const auto [next, rev] = full_next(res, true);
                if (!rev || !next->parent)
                    return next;
                res = next;
            }
        }

        constexpr static node_base* pre_prev(const node_base* ptr)
        {
            auto [res, revisit] = full_prev(ptr, !ptr->parent);
            if (!revisit) return res;
            while (true)
            {
                const auto [prev, rev] = full_prev(res, true);
                if (!rev) return prev;
                res = prev;
            }
        }

        class iter_impl : public iter_base
        {
        public:
            using iter_base::iter_base;
            constexpr explicit(false) operator const_iter_impl() const noexcept { return const_iter_impl(this->ptr_); }

            [[nodiscard]] constexpr friend bool operator==(iter_impl, iter_impl) noexcept = default;

            constexpr iter_impl& operator++() noexcept
            {
                this->ptr_ = pre_next(this->ptr_);
                return *this;
            }

            constexpr iter_impl& operator--() noexcept
            {
                this->ptr_ = pre_prev(this->ptr_);
                return *this;
            }
        };

        class const_iter_impl : public const_iter_base
        {
        public:
            using const_iter_base::const_iter_base;

            [[nodiscard]] constexpr friend bool operator==(const_iter_impl, const_iter_impl) noexcept = default;

            constexpr const_iter_impl& operator++() noexcept
            {
                this->ptr_ = pre_next(this->ptr_);
                return *this;
            }

            constexpr const_iter_impl& operator--() noexcept
            {
                this->ptr_ = pre_prev(this->ptr_);
                return *this;
            }
        };

        class full_iter_impl : public iter_base
        {
        private:
            bool revisit_ = false;

        public:
            constexpr full_iter_impl() = default;
            constexpr full_iter_impl(node_base* ptr, const bool revisit) noexcept: iter_base(ptr), revisit_(revisit) {}
            constexpr explicit(false) operator const_full_iter_impl() const noexcept { return { this->ptr_, revisit_ }; }

            [[nodiscard]] constexpr bool operator==(const full_iter_impl&) const noexcept = default;
            [[nodiscard]] constexpr bool is_revisit() const noexcept { return revisit_; }

            constexpr full_iter_impl& operator++() noexcept
            {
                std::tie(this->ptr_, revisit_) = full_next(this->ptr_, revisit_);
                return *this;
            }

            constexpr full_iter_impl& operator--() noexcept
            {
                std::tie(this->ptr_, revisit_) = full_prev(this->ptr_, revisit_);
                return *this;
            }
        };

        class const_full_iter_impl : public const_iter_base
        {
        private:
            bool revisit_ = false;

        public:
            constexpr const_full_iter_impl() = default;
            constexpr const_full_iter_impl(const node_base* ptr, const bool revisit) noexcept: const_iter_base(ptr), revisit_(revisit) {}

            [[nodiscard]] constexpr bool operator==(const const_full_iter_impl&) const noexcept = default;
            [[nodiscard]] constexpr bool is_revisit() const noexcept { return revisit_; }

            constexpr const_full_iter_impl& operator++() noexcept
            {
                std::tie(this->ptr_, revisit_) = full_next(this->ptr_, revisit_);
                return *this;
            }

            constexpr const_full_iter_impl& operator--() noexcept
            {
                std::tie(this->ptr_, revisit_) = full_prev(this->ptr_, revisit_);
                return *this;
            }
        };

        class child_iter_impl : public iter_base
        {
        private:
            bool end_ = false;

        public:
            constexpr child_iter_impl() = default;
            constexpr child_iter_impl(node_base* ptr, const bool end) noexcept: iter_base(ptr), end_(end) {}
            constexpr explicit(false) operator const_child_iter_impl() const noexcept { return { this->ptr_, end_ }; }

            [[nodiscard]] constexpr bool operator==(const child_iter_impl&) const noexcept = default;

            constexpr child_iter_impl& operator++() noexcept
            {
                this->ptr_ = this->ptr_->derived()->next;
                end_ = this->ptr_->derived()->is_first_sibling();
                return *this;
            }

            constexpr child_iter_impl& operator--() noexcept
            {
                this->ptr_ = this->ptr_->derived()->prev;
                end_ = false;
                return *this;
            }
        };

        class const_child_iter_impl : public const_iter_base
        {
        private:
            bool end_ = false;

        public:
            constexpr const_child_iter_impl() = default;
            constexpr const_child_iter_impl(const node_base* ptr, const bool end) noexcept: const_iter_base(ptr), end_(end) {}

            [[nodiscard]] constexpr bool operator==(const const_child_iter_impl&) const noexcept = default;

            constexpr const_child_iter_impl& operator++() noexcept
            {
                this->ptr_ = this->ptr_->derived()->next;
                end_ = this->ptr_->derived()->is_first_sibling();
                return *this;
            }

            constexpr const_child_iter_impl& operator--() noexcept
            {
                this->ptr_ = this->ptr_->derived()->prev;
                end_ = false;
                return *this;
            }
        };

        using al_traits = std::allocator_traits<Alloc>;
        using al_node = typename al_traits::template rebind_alloc<node>;
        using al_node_traits = std::allocator_traits<al_node>;

        static constexpr bool pocca = al_traits::propagate_on_container_copy_assignment::value;
        static constexpr bool pocma = al_traits::propagate_on_container_move_assignment::value;
        static constexpr bool pocs = al_traits::propagate_on_container_swap::value;
        static constexpr bool al_always_equal = al_traits::is_always_equal::value;

        [[no_unique_address]] al_node alloc_{};
        node_base root_;

        template <typename... Us>
        constexpr node* new_node(Us&&... args)
        {
            node* ptr = al_node_traits::allocate(alloc_, 1);
            al_node_traits::construct(alloc_, std::to_address(ptr), std::forward<Us>(args)...);
            ptr->prev = ptr->next = ptr;
            return ptr;
        }

        constexpr void delete_node(node* ptr) noexcept
        {
            al_node_traits::destroy(alloc_, std::to_address(ptr));
            al_node_traits::deallocate(alloc_, ptr, 1);
        }

        template <typename F> // F: node_base* -> node*
        constexpr static void copy_children_impl(node_base& dst, const node_base& src, F copy)
        {
            if (!src.child) return;
            node* current = dst.derived();
            const node* other_current = src.derived();
            bool backtracking = false;
            do
            {
                if (!backtracking && other_current->child)
                {
                    const auto child = current->child = copy(other_current->child);
                    child->parent = current;
                    current = child;
                    other_current = other_current->child;
                }
                else if (!other_current->is_last_sibling())
                {
                    const auto sibling = current->next = copy(other_current->next);
                    sibling->parent = current->parent;
                    sibling->prev = current;
                    const auto first = current->parent->child;
                    sibling->next = first;
                    first->prev = sibling;
                    current = sibling;
                    other_current = other_current->next;
                    backtracking = false;
                }
                else
                {
                    backtracking = true;
                    other_current = other_current->parent->derived();
                    current = current->parent->derived();
                }
            } while (other_current != &src);
        }

        constexpr full_order_iterator full_begin()
        {
            return root_.child ?
                       full_order_iterator(root_.child, false) :
                       full_order_iterator(&root_, true);
        }

        constexpr const_full_order_iterator full_begin() const
        {
            return root_.child ?
                       const_full_order_iterator(root_.child, false) :
                       const_full_order_iterator(&root_, true);
        }

        constexpr void attach_siblings_after_no_check(const const_iter_base it, forest&& other,
            const bool reset_first = false) noexcept
        {
            if (!other.root_.child) return;
            node* prev = static_cast<node*>(const_cast<node_base*>(it.ptr_));
            node_base* parent = prev->parent;
            node* next = prev->next;
            node* added_first = std::exchange(other.root_.child, nullptr);
            node* added_last = added_first->prev;
            if (reset_first)
                parent->child = added_first;
            prev->next = added_first;
            next->prev = added_last;
            added_last->next = next;
            added_first->prev = prev;
            for (node* child = added_first; child != next; child = child->next)
                child->parent = parent;
        }

        constexpr void attach_children_to_leaf(const const_iter_base it, forest&& other) noexcept
        {
            if (!other.root_.child) return;
            auto* ptr = const_cast<node_base*>(it.ptr_);
            ptr->child = std::exchange(other.root_.child, nullptr);
            ptr->fix_children_parents();
        }

        constexpr void attach_children_back_no_check(const const_iter_base it, forest&& other) noexcept
        {
            if (it.is_leaf())
                attach_children_to_leaf(it, std::move(other));
            else
                attach_siblings_after_no_check(const_iter_base(it.ptr_->child->prev), std::move(other));
        }

        constexpr void attach_children_front_no_check(const const_iter_base it, forest&& other) noexcept
        {
            if (it.is_leaf())
                attach_children_to_leaf(it, std::move(other));
            else
                attach_siblings_after_no_check(
                    const_iter_base(it.ptr_->child->prev), std::move(other), true);
        }

        constexpr void attach_siblings_before_no_check(const const_iter_base it, forest&& other) noexcept
        {
            attach_siblings_after_no_check(
                it.prev_sibling(), std::move(other), it.ptr_->parent->child == it.ptr_);
        }

    public:
        constexpr forest() noexcept(noexcept(al_node())) = default;
        constexpr explicit forest(const Alloc& alloc) noexcept: alloc_(alloc) {}

        constexpr forest(const forest& other):
            forest(other, std::allocator_traits<Alloc>::select_on_container_copy_construction(Alloc(other.alloc_))) {}

        constexpr forest(const forest& other, const Alloc& alloc): alloc_(alloc)
        {
            copy_children_impl(root_, other.root_,
                [&](node_base* ptr) { return new_node(ptr->derived()->value); });
        }

        constexpr forest(forest&& other) noexcept:
            alloc_(std::move(other.alloc_)),
            root_(clu::take(other.root_)) { root_.fix_children_parents(); }

        constexpr forest(forest&& other, const Alloc& alloc)
        {
            if (alloc == Alloc(alloc_))
            {
                root_ = clu::take(other.root_);
                root_.fix_children_parents();
            }
            else
            {
                alloc_ = al_node(alloc);
                copy_children_impl(root_, other.root_,
                    [&](node_base* ptr) { return new_node(std::move(ptr->derived()->value)); });
            }
        }

        constexpr ~forest() noexcept { clear(); }

        constexpr forest& operator=(const forest& other)
        {
            if (&other != this)
            {
                clear();
                copy_children_impl(root_, other.root_,
                    [&](node_base* ptr) { return new_node(ptr->derived()->value); });
            }
            return *this;
        }

        constexpr forest& operator=(forest&& other) noexcept(pocma || al_always_equal)
        {
            if (&other != this)
            {
                clear();
                if constexpr (al_always_equal || pocma)
                {
                    if constexpr (!al_always_equal) alloc_ = other.alloc_;
                    root_ = clu::take(other.root_);
                    root_.fix_children_parents();
                }
                else if (alloc_ == other.alloc_)
                {
                    root_ = clu::take(other.root_);
                    root_.fix_children_parents();
                }
                else
                    copy_children_impl(root_, other.root_,
                        [&](node_base* ptr) { return new_node(std::move(ptr->derived()->value)); });
            }
            return *this;
        }

        constexpr allocator_type get_allocator() const noexcept { return allocator_type(alloc_); }

        [[nodiscard]] constexpr iterator begin() noexcept { return iterator(root_.child ? root_.child : &root_); }
        [[nodiscard]] constexpr iterator end() noexcept { return iterator(&root_); }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(root_.child ? root_.child : &root_); }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(&root_); }
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
        [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }
        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr reverse_iterator rend() noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return std::make_reverse_iterator(begin()); }

        [[nodiscard]] constexpr full_order_range full_order_traverse() noexcept { return { full_begin(), { &root_, true } }; }
        [[nodiscard]] constexpr const_full_order_range full_order_traverse() const noexcept { return { full_begin(), { &root_, true } }; }

        [[nodiscard]] constexpr bool empty() const noexcept { return !root_.child; }

        template <typename... Ts>
        constexpr iterator emplace_child_back(const const_iter_base it, Ts&&... args)
        {
            if (auto* parent = const_cast<node_base*>(it.ptr_);
                !parent->child)
            {
                node* child = new_node(std::forward<Ts>(args)...);
                parent->child = child;
                child->parent = parent;
                return iterator(child);
            }
            else
            {
                node* first = parent->child;
                node* last = new_node(std::forward<Ts>(args)...);
                last->prev = std::exchange(first->prev, last);
                last->next = first;
                last->prev->next = last;
                last->parent = parent;
                return iterator(last);
            }
        }
        constexpr iterator insert_child_back(const const_iter_base it, const T& value) { return emplace_child_back(it, value); }
        constexpr iterator insert_child_back(const const_iter_base it, T&& value) { return emplace_child_back(it, std::move(value)); }

        template <typename... Ts>
        constexpr iterator emplace_child_front(const const_iter_base it, Ts&&... args)
        {
            if (auto* parent = const_cast<node_base*>(it.ptr_);
                !parent->child)
            {
                node* child = new_node(std::forward<Ts>(args)...);
                parent->child = child;
                child->parent = parent;
                return iterator(child);
            }
            else
            {
                node* first = parent->child;
                node* added = new_node(std::forward<Ts>(args)...);
                parent->child = added;
                added->parent = parent;
                added->prev = std::exchange(first->prev, added);
                added->prev->next = added;
                added->next = first;
                return iterator(added);
            }
        }
        constexpr iterator insert_child_front(const const_iter_base it, const T& value) { return emplace_child_front(it, value); }
        constexpr iterator insert_child_front(const const_iter_base it, T&& value) { return emplace_child_front(it, std::move(value)); }

        template <std::input_iterator It, std::sentinel_for<It> Se>
            requires std::convertible_to<std::iter_value_t<It>, T>
        constexpr iterator insert_children_back(const const_iter_base it, It begin, const Se end)
        {
            if (begin == end) return iterator(const_cast<node_base*>(it.ptr_));
            iterator first = insert_child_back(it, *begin++);
            insert_siblings_after(first, begin, end);
            return first;
        }
        template <std::ranges::input_range Rng>
            requires std::convertible_to<std::ranges::range_value_t<Rng>, T> && (!clu::similar_to<Rng, forest>)
        constexpr iterator insert_children_back(const const_iter_base it, Rng&& range)
        {
            return insert_children_back(it, std::ranges::begin(range), std::ranges::end(range));
        }
        constexpr iterator insert_children_back(const const_iter_base it, const std::initializer_list<T> list)
        {
            return insert_children_back(it, list.begin(), list.end());
        }

        template <std::input_iterator It, std::sentinel_for<It> Se>
            requires std::convertible_to<std::iter_value_t<It>, T>
        constexpr iterator insert_children_front(const const_iter_base it, It begin, const Se end)
        {
            if (begin == end) return iterator(const_cast<node_base*>(it.ptr_));
            iterator first = insert_child_front(it, *begin++);
            insert_siblings_after(first, begin, end);
            return first;
        }
        template <std::ranges::input_range Rng>
            requires std::convertible_to<std::ranges::range_value_t<Rng>, T>
        constexpr iterator insert_children_front(const const_iter_base it, Rng&& range)
        {
            return insert_children_front(it, std::ranges::begin(range), std::ranges::end(range));
        }
        constexpr iterator insert_children_front(const const_iter_base it, const std::initializer_list<T> list)
        {
            return insert_children_front(it, list.begin(), list.end());
        }

        template <typename... Ts>
        constexpr iterator emplace_sibling_after(const const_iter_base it, Ts&&... args)
        {
            CLU_ASSERT(it.ptr_ != &root_, "cannot emplace sibling for the end iterator");
            node* ptr = static_cast<node*>(const_cast<node_base*>(it.ptr_));
            node* added = new_node(std::forward<Ts>(args)...);
            ptr->next->prev = added;
            added->next = std::exchange(ptr->next, added);
            added->prev = ptr;
            added->parent = ptr->parent;
            return iterator(added);
        }
        constexpr iterator insert_sibling_after(const const_iter_base it, const T& value) { return emplace_sibling_after(it, value); }
        constexpr iterator insert_sibling_after(const const_iter_base it, T&& value)
        {
            return emplace_sibling_after(it, std::move(value));
        }

        template <typename... Ts>
        constexpr iterator emplace_sibling_before(const const_iter_base it, Ts&&... args)
        {
            CLU_ASSERT(it.ptr_ != &root_, "cannot emplace sibling for the end iterator");
            node* ptr = static_cast<node*>(const_cast<node_base*>(it.ptr_));
            node* added = new_node(std::forward<Ts>(args)...);
            ptr->prev->next = added;
            added->prev = std::exchange(ptr->prev, added);
            added->next = ptr;
            node_base* parent = ptr->parent;
            added->parent = parent;
            if (parent->child == ptr) parent->child = added;
            return iterator(added);
        }
        constexpr iterator insert_sibling_before(const const_iter_base it, const T& value) { return emplace_sibling_before(it, value); }
        constexpr iterator insert_sibling_before(const const_iter_base it, T&& value)
        {
            return emplace_sibling_before(it, std::move(value));
        }

        template <std::input_iterator It, std::sentinel_for<It> Se>
            requires std::convertible_to<std::iter_value_t<It>, T>
        constexpr iterator insert_siblings_after(const const_iter_base it, It begin, const Se end)
        {
            // TODO: how hard would it be to filfull stronger exception guarantees for some operations?
            node* prev = static_cast<node*>(const_cast<node_base*>(it.ptr_));
            if (begin == end) return iterator(prev);
            node* last = prev->next;

            node* res_node = new_node(*begin++);
            res_node->prev = prev;
            prev->next = res_node;
            res_node->parent = prev->parent;
            prev = res_node;
            for (; begin != end; ++begin)
            {
                node* added = new_node(*begin);
                added->prev = prev;
                prev->next = added;
                added->parent = prev->parent;
                prev = added;
            }
            last->prev = prev;
            prev->next = last;

            return iterator(res_node);
        }
        template <std::ranges::input_range Rng>
            requires std::convertible_to<std::ranges::range_value_t<Rng>, T> && (!clu::similar_to<Rng, forest>)
        constexpr iterator insert_siblings_after(const const_iter_base it, Rng&& range)
        {
            return insert_siblings_after(it, std::ranges::begin(range), std::ranges::end(range));
        }
        constexpr iterator insert_siblings_after(const const_iter_base it, const std::initializer_list<T> list)
        {
            return insert_siblings_after(it, list.begin(), list.end());
        }

        template <std::input_iterator It, std::sentinel_for<It> Se>
            requires std::convertible_to<std::iter_value_t<It>, T>
        constexpr iterator insert_siblings_before(const const_iter_base it, It begin, const Se end)
        {
            CLU_ASSERT(it.ptr_ != &root_, "cannot insert siblings for the end iterator");
            node* ptr = static_cast<node*>(const_cast<node_base*>(it.ptr_));
            if (begin == end) return iterator(ptr);
            if (ptr->is_first_sibling())
            {
                const iterator first = insert_child_front(it.parent(), *begin++);
                insert_siblings_after(first, begin, end);
                return first;
            }
            return insert_siblings_after(it.prev_sibling(), begin, end);
        }
        template <std::ranges::input_range Rng>
            requires std::convertible_to<std::ranges::range_value_t<Rng>, T> && (!clu::similar_to<Rng, forest>)
        constexpr iterator insert_siblings_before(const const_iter_base it, Rng&& range)
        {
            return insert_siblings_before(it, std::ranges::begin(range), std::ranges::end(range));
        }
        constexpr iterator insert_siblings_before(const const_iter_base it, const std::initializer_list<T> list)
        {
            return insert_siblings_before(it, list.begin(), list.end());
        }

        constexpr forest detach(const const_iter_base it) noexcept
        {
            CLU_ASSERT(it.ptr_ != &root_, "cannot detach end()");
            node* ptr = static_cast<node*>(const_cast<node_base*>(it.ptr_));
            if (ptr->is_first_sibling())
                ptr->parent->child = ptr->next == ptr ? nullptr : ptr->next;
            ptr->prev->next = ptr->next;
            ptr->next->prev = ptr->prev;
            ptr->prev = ptr->next = ptr;
            forest result{ Alloc(alloc_) };
            result.root_.child = ptr;
            ptr->parent = &result.root_;
            return result;
        }

        constexpr forest detach_children(const const_iter_base it) noexcept
        {
            if (it.is_leaf()) return forest(Alloc(alloc_));
            node* first = std::exchange(const_cast<node_base*>(it.ptr_)->child, nullptr);
            forest result{ Alloc(alloc_) };
            result.root_.child = first;
            result.root_.fix_children_parents();
            return result;
        }

        constexpr forest copy_subtree(const const_iter_base it) const { return copy_subtree(it, Alloc(alloc_)); }
        constexpr forest copy_subtree(const const_iter_base it, const Alloc alloc) const
        {
            CLU_ASSERT(it.ptr_ != &root_, "cannot copy subtree at end()");
            forest result(alloc);
            node* tree_root = result.new_node(*it);
            result.root_.child = tree_root;
            tree_root->parent = &result.root_;
            copy_children_impl(*tree_root, *(it.ptr_),
                [&](const node_base* ptr) { return result.new_node(ptr->derived()->value); });
            return result;
        }

        constexpr forest copy_children(const const_iter_base it) const { return copy_children(it, Alloc(alloc_)); }
        constexpr forest copy_children(const const_iter_base it, const Alloc alloc) const
        {
            forest result(alloc);
            copy_children_impl(result.root_, *(it.ptr_),
                [&](const node_base* ptr) { return result.new_node(ptr->derived()->value); });
            return result;
        }

        constexpr void attach_children_back(const const_iter_base it, const forest& other) noexcept
        {
            attach_children_back_no_check(it, forest(other, Alloc(alloc_)));
        }
        constexpr void attach_children_back(const const_iter_base it, forest&& other) noexcept(al_always_equal)
        {
            if (al_always_equal || alloc_ == other.alloc_)
                attach_children_back_no_check(it, std::move(other));
            else
                attach_children_back_no_check(it, forest(std::move(other), Alloc(alloc_)));
        }

        constexpr void attach_children_front(const const_iter_base it, const forest& other)
        {
            attach_children_front_no_check(it, forest(other, Alloc(alloc_)));
        }
        constexpr void attach_children_front(const const_iter_base it, forest&& other) noexcept(al_always_equal)
        {
            if (al_always_equal || alloc_ == other.alloc_)
                attach_children_front_no_check(it, std::move(other));
            else
                attach_children_front_no_check(it, forest(std::move(other), Alloc(alloc_)));
        }

        constexpr void attach_siblings_after(const const_iter_base it, const forest& other)
        {
            attach_siblings_after_no_check(it, forest(other, Alloc(alloc_)));
        }
        constexpr void attach_siblings_after(const const_iter_base it, forest&& other) noexcept(al_always_equal)
        {
            if (al_always_equal || alloc_ == other.alloc_)
                attach_siblings_after_no_check(it, std::move(other));
            else
                attach_siblings_after_no_check(it, forest(std::move(other), Alloc(alloc_)));
        }

        constexpr void attach_siblings_before(const const_iter_base it, const forest& other)
        {
            attach_siblings_before_no_check(it, forest(other, Alloc(alloc_)));
        }
        constexpr void attach_siblings_before(const const_iter_base it, forest&& other) noexcept(al_always_equal)
        {
            if (al_always_equal || alloc_ == other.alloc_)
                attach_siblings_before_no_check(it, std::move(other));
            else
                attach_siblings_before_no_check(it, forest(std::move(other), Alloc(alloc_)));
        }

        constexpr void clear() noexcept
        {
            if (!root_.child) return;
            constexpr auto right_bottom_most_node = [](node_base* ptr)
            {
                while (ptr->child) ptr = ptr->child->prev;
                return ptr->derived();
            };
            auto ptr = right_bottom_most_node(&root_);
            while (true)
            {
                if (!ptr->is_first_sibling())
                {
                    const auto sibling = ptr->prev;
                    delete_node(ptr);
                    ptr = right_bottom_most_node(sibling);
                }
                else if (ptr->parent != &root_)
                {
                    const auto parent = ptr->parent->derived();
                    delete_node(ptr);
                    ptr = parent;
                }
                else
                {
                    delete_node(ptr);
                    return;
                }
            }
        }

        constexpr void swap(forest& other) noexcept(pocs || al_always_equal)
        {
            if constexpr (pocs) std::swap(alloc_, other.alloc_);
            std::swap(root_, other.root_);
            root_.fix_children_parents();
            other.root_.fix_children_parents();
        }

        constexpr friend void swap(forest& lhs, forest& rhs) noexcept(pocs || al_always_equal) { lhs.swap(rhs); }

        [[nodiscard]] constexpr bool is_leaf(const const_iter_base it) const noexcept { return it.ptr_->child == nullptr; }
        [[nodiscard]] constexpr bool is_root(const const_iter_base it) const noexcept { return it.ptr_->parent == &root_; }
        [[nodiscard]] constexpr iterator parent_of(const const_iter_base it) noexcept { return iter_base(it).parent(); }
        [[nodiscard]] constexpr const_iterator parent_of(const const_iter_base it) const noexcept { return it.parent(); }
        [[nodiscard]] constexpr iterator next_sibling_of(const const_iter_base it) noexcept { return iter_base(it).next_sibling(); }
        [[nodiscard]] constexpr const_iterator next_sibling_of(const const_iter_base it) const noexcept { return it.next_sibling(); }
        [[nodiscard]] constexpr iterator prev_sibling_of(const const_iter_base it) noexcept { return iter_base(it).prev_sibling(); }
        [[nodiscard]] constexpr const_iterator prev_sibling_of(const const_iter_base it) const noexcept { return it.prev_sibling(); }
        [[nodiscard]] constexpr child_range roots() noexcept { return end().children(); }
        [[nodiscard]] constexpr const_child_range roots() const noexcept { return end().children(); }
        [[nodiscard]] constexpr child_range children_of(const const_iter_base it) noexcept { return iter_base(it).children(); }
        [[nodiscard]] constexpr const_child_range children_of(const const_iter_base it) const noexcept { return it.children(); }
    };

    namespace pmr
    {
        template <typename T>
        using forest = forest<T, std::pmr::polymorphic_allocator<T>>;
    }
}
