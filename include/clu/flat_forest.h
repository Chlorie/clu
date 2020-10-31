#pragma once

#include <utility>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace clu
{
    // TODO: Finish doc
    /**
     * \brief A forest data structure implementation with a contiguous buffer for storing the nodes
     * \tparam T The type of the value stored in the nodes
     */
    template <typename T>
    class flat_forest final
    {
    public:
        using value_type = T; ///< The type of the value stored in the nodes

    private:
        static constexpr size_t null_index = static_cast<size_t>(-1);

        struct node final
        {
            union storage // NOLINT(cppcoreguidelines-special-member-functions)
            {
                size_t next_free;
                T value;

                storage() noexcept: next_free{ null_index } {}
                ~storage() noexcept {}

                template <typename... Ts>
                T& emplace(Ts&&... args) noexcept(std::is_nothrow_constructible_v<T, Ts&&...>)
                {
                    return *new(&value) T(std::forward<Ts>(args)...);
                }

                void destroy() noexcept { value.~T(); }

                T& operator*() noexcept { return value; }
                const T& operator*() const noexcept { return value; }
            } value;

            size_t parent = null_index; // Used as next free node index when free
            size_t first_child = null_index;
            size_t next_sibling = null_index;
        };
        friend struct node;

        static constexpr bool over_aligned = alignof(node) > alignof(std::max_align_t);
        static constexpr auto node_align_val = static_cast<std::align_val_t>(alignof(node));

        std::unique_ptr<node[]> data_ = nullptr;
        size_t size_ = 0;
        size_t capacity_ = 0;
        size_t first_root_ = null_index;
        size_t free_list_ = null_index;

        template <bool IsConst>
        class iterator_impl final
        {
            friend class iterator_impl<true>;
            friend class flat_forest;
        private:
            using qual_value = std::conditional_t<IsConst, const T, T>;
            using qual_node = std::conditional_t<IsConst, const node, node>;
            using qual_forest = std::conditional_t<IsConst, const flat_forest, flat_forest>;

        public:
            using difference_type = ptrdiff_t;
            using value_type = T;
            using pointer = qual_value*;
            using reference = qual_value&;
            using iterator_category = std::forward_iterator_tag;

        private:
            qual_forest* ptr_ = nullptr;
            size_t index_ = null_index;

            qual_node& get() const noexcept { return ptr_->data_[index_]; }

        public:
            iterator_impl() noexcept = default;
            iterator_impl(const iterator_impl<false>& it) noexcept: ptr_(it.ptr_), index_(it.index_) {}
            iterator_impl(qual_forest* const ptr, const size_t index) noexcept: ptr_(ptr), index_(index) {}

            friend bool operator==(const iterator_impl lhs, const iterator_impl rhs) noexcept
            {
                return lhs.ptr_ == rhs.ptr_ && lhs.index_ == rhs.index_;
            }
            friend bool operator!=(const iterator_impl lhs, const iterator_impl rhs) noexcept
            {
                return lhs.ptr_ != rhs.ptr_ || lhs.index_ != rhs.index_;
            }

            reference operator*() const noexcept { return *get().value; }
            pointer operator->() const noexcept { return &*get().value; }

            iterator_impl& operator++() noexcept
            {
                const node& ref = get();
                if (ref.first_child != null_index) // Has children
                    index_ = ref.first_child;
                else // Leaf node
                {
                    const node* ancestor = &ref;
                    while (ancestor->next_sibling == null_index
                        && ancestor->parent != null_index)
                        ancestor = &ptr_->data_[ancestor->parent];
                    index_ = ancestor->next_sibling; // Ancestor is root of some tree or has more siblings
                }
                return *this;
            }

            iterator_impl operator++(int) noexcept
            {
                iterator_impl result = *this;
                operator++();
                return result;
            }

            iterator_impl next() const noexcept
            {
                iterator_impl result = *this;
                return ++result;
            }

            qual_forest& forest() const noexcept { return *ptr_; } ///< Get the forest from which this iterator is
            bool is_sentinel() const noexcept { return index_ == null_index; } ///< Checks if this iterator is the end

            bool is_root() const noexcept { return get().parent == null_index; } ///< Checks if this iterator points to a root node
            bool is_leaf() const noexcept { return get().first_child == null_index; } ///< Checks if this iterator points to a leaf node
            bool is_last_child_of_parent() const noexcept { return get().next_sibling == null_index; }

            iterator_impl parent() const noexcept { return { ptr_, get().parent }; }
            iterator_impl first_child() const noexcept { return { ptr_, get().first_child }; }
            iterator_impl next_sibling() const noexcept { return { ptr_, get().next_sibling }; }

            template <typename Func>
            void for_each_child(Func&& func) const noexcept(func(std::declval<iterator_impl>()))
            {
                for (iterator_impl it = first_child(); !it.is_sentinel(); it = it.next_sibling())
                    func(it);
            }
        };
        template <bool IsConst> friend class iterator_impl;

        void destroy_values() noexcept
        {
            preorder_traverse(
                [](const iterator it) noexcept { it.get().value.destroy(); });
        }

        template <typename U = const T&> // Use U = T&& for moving
        static void copy_structure_to(const flat_forest& src, node* dst)
        noexcept(std::is_nothrow_constructible_v<T, U>)
        {
            if (src.empty()) return;
            node* ptr = &src.data_[src.first_root_];
            dst[0].value.emplace(static_cast<U>(*ptr->value)); // First root
            size_t index = 0;
            try
            {
                while (true)
                {
                    if (ptr->first_child != null_index) // Has children
                    {
                        dst[index].first_child = index + 1;
                        dst[index + 1].parent = index;
                        ptr = &src.data_[ptr->first_child];
                    }
                    else // Leaf, find first ancestor (including itself) with next sibling
                    {
                        size_t ancestor = index;
                        while (ptr->next_sibling == null_index
                            && ptr->parent != null_index)
                        {
                            dst[ancestor].next_sibling = null_index;
                            ancestor = dst[ancestor].parent;
                            ptr = &src.data_[ptr->parent];
                        }
                        if (ptr->parent == null_index)
                        {
                            dst[ancestor].next_sibling = null_index;
                            break; // Last node
                        }
                        dst[ancestor].next_sibling = index + 1;
                        dst[index + 1].parent = dst[ancestor].parent;
                        ptr = &src.data_[ptr->next_sibling];
                    }
                    dst[++index].value.emplace(static_cast<U>(*ptr->value));
                }
            }
            catch (...) // Some emplacement failed, destroy all
            {
                for (size_t i = 0; i < index; i++)
                    dst[i].value.destroy();
                throw;
            }
        }

        // Add data_[start .. capacity_] to free list 
        void add_free_range(const size_t start) noexcept
        {
            for (size_t i = start; i != capacity_; i++)
                data_[i].value.next_free = i + 1;
            data_[capacity_ - 1].value.next_free = std::exchange(free_list_, start);
        }

        void resize_buffer(const size_t new_size)
        {
            auto new_data = std::make_unique<node[]>(new_size);
            copy_structure_to<T&&>(*this, new_data.get());
            data_ = std::move(new_data);
            capacity_ = new_size;
            first_root_ = empty() ? null_index : 0;
            free_list_ = null_index;
            if (size_ < capacity_) add_free_range(size_);
        }

        void detach_node_from_parent(const size_t index) noexcept
        {
            node& current = data_[index];
            size_t& first = current.parent == null_index
                                ? first_root_
                                : data_[current.parent].first_child;
            if (first == index)
                first = std::exchange(current.next_sibling, null_index);
            else
            {
                node* ptr = &data_[first];
                while (ptr->next_sibling != index)
                    ptr = &data_[ptr->next_sibling];
                ptr->next_sibling = std::exchange(current.next_sibling, null_index);
            }
            current.parent = null_index;
        }

        size_t get_free_node()
        {
            if (free_list_ == null_index) // Buffer is full
            {
                const size_t growth = capacity_ + capacity_ / 2;
                resize_buffer(std::max(capacity_ + 1, growth));
            }
            return std::exchange(free_list_, data_[free_list_].value.next_free);
        }

    public:
        using iterator = iterator_impl<false>; ///< Iterator type which allows read/write access to the nodes
        using const_iterator = iterator_impl<true>; ///< Iterator type which allows read only access to the nodes

        flat_forest() noexcept = default; ///< Constructs an empty forest

        ~flat_forest() noexcept { destroy_values(); } ///< Destroys the forest

        /**
         * \brief Construct a copy of another forest
         */
        flat_forest(const flat_forest& other):
            data_(std::make_unique<node[]>(other.size_)),
            size_(other.size_), capacity_(other.capacity_), first_root_(0)
        {
            copy_structure_to(other, data_.get());
        }

        /**
         * \brief Construct a forest by moving another forest's content
         */
        flat_forest(flat_forest&& other) noexcept:
            data_(std::move(other.data_)),
            size_(std::exchange(other.size_, 0)),
            capacity_(std::exchange(other.capacity_, 0)),
            first_root_(std::exchange(other.first_root_, null_index)),
            free_list_(std::exchange(other.free_list_, null_index)) {}

        /**
         * \brief Copy another forest to this
         */
        flat_forest& operator=(const flat_forest& other)
        {
            flat_forest copy = other;
            swap(copy);
            return *this;
        }

        /**
         * \brief Move another forest to this
         */
        flat_forest& operator=(flat_forest&& other) noexcept
        {
            if (&other == this) return *this;
            data_ = std::move(other.data_);
            size_ = std::exchange(other.size_, 0);
            capacity_ = std::exchange(other.capacity_, 0);
            first_root_ = std::exchange(other.first_root_, null_index);
            free_list_ = std::exchange(other.free_list_, null_index);
            return *this;
        }

        /**
         * \brief Swap this forest with another
         */
        void swap(flat_forest& other) noexcept
        {
            std::swap(data_, other.data_);
            std::swap(size_, other.size_);
            std::swap(capacity_, other.capacity_);
            std::swap(first_root_, other.first_root_);
            std::swap(free_list_, other.free_list_);
        }

        friend void swap(flat_forest& lhs, flat_forest& rhs) noexcept { lhs.swap(rhs); } ///< Swap two forests

        size_t size() const noexcept { return size_; } ///< Amount of nodes stored
        size_t capacity() const noexcept { return capacity_; } ///< Capacity of current buffer
        bool empty() const noexcept { return size_ == 0; } ///< Checks if this forest is empty

        /**
         * \brief Expand the buffer to a larger capacity
         */
        void reserve(const size_t new_capacity)
        {
            if (new_capacity > capacity_)
                resize_buffer(new_capacity);
        }

        void shrink_to_fit() { resize_buffer(size_); } ///< Shrink the buffer to exactly fit the nodes

        /**
         * \brief Destroy all nodes in this forest
         */
        void clear()
        {
            preorder_traverse([&](const iterator it)
            {
                node& n = it.get();
                n.value.destroy();
                n.value.next_free = std::exchange(free_list_, it.index_);
            });
            size_ = 0;
            first_root_ = null_index;
        }

        iterator begin() noexcept { return iterator(this, first_root_); } ///< Begin iterator
        const_iterator begin() const noexcept { return const_iterator(this, first_root_); } ///< Begin const iterator
        const_iterator cbegin() const noexcept { return const_iterator(this, first_root_); } ///< Begin const iterator
        iterator end() noexcept { return iterator(this, null_index); } ///< End iterator
        const_iterator end() const noexcept { return const_iterator(this, null_index); } ///< End const iterator
        const_iterator cend() const noexcept { return const_iterator(this, null_index); } ///< End const iterator

        ///@{
        /**
         * \brief Pre-order traverses the forest and calls a function object on each of the node's iterator
         * \param func The function object to call on the iterators
         */
        template <typename Func>
        void preorder_traverse(Func&& func) noexcept(noexcept(func(std::declval<iterator>())))
        {
            for (iterator it = begin(); !it.is_sentinel(); ++it)
                func(it);
        }

        template <typename Func>
        void preorder_traverse(Func&& func) const noexcept(noexcept(func(std::declval<const_iterator>())))
        {
            for (const_iterator it = begin(); !it.is_sentinel(); ++it)
                func(it);
        }
        ///@}

        void prune(const const_iterator branch)
        {
            detach_node_from_parent(branch.index_);
            for (iterator it{ this, branch.index_ }; !it.is_sentinel(); ++it)
            {
                auto& value = it.get().value;
                value.destroy();
                value.next_free = std::exchange(free_list_, it.index_);
                --size_;
            }
        }

        void reset_parent(const const_iterator branch, const const_iterator new_parent)
        {
            const size_t index = branch.index_;
            detach_node_from_parent(index);
            if (first_root_ == null_index) // The branch is the whole forest
                first_root_ = index;
            else if (new_parent.is_sentinel()) // Detach as new tree
            {
                node* ptr = &data_[first_root_];
                while (ptr->next_sibling != null_index) ptr = &data_[ptr->next_sibling];
                ptr->next_sibling = index;
            }
            else // Attach to existing node
            {
                node& branch_ref = data_[index];
                branch_ref.parent = new_parent.index_;
                branch_ref.next_sibling = std::exchange(data_[new_parent.index_].first_child, index);
            }
        }

        template <typename... Ts>
        iterator emplace_child(const const_iterator parent, Ts&&... args)
        {
            const size_t free_node = get_free_node();
            node& ref = data_[free_node];
            try { ref.value.emplace(std::forward<Ts>(args)...); }
            catch (...) // Failed to emplace, re-free the node
            {
                ref.value.next_free = std::exchange(free_list_, free_node);
                throw;
            }
            ref.first_child = null_index;
            ref.parent = parent.index_;
            size_t& first_child_of_parent = parent.is_sentinel()
                                                ? first_root_
                                                : data_[parent.index_].first_child;
            ref.next_sibling = std::exchange(first_child_of_parent, free_node);
            ++size_;
            return { this, free_node };
        }

        template <typename... Ts>
        iterator emplace_sibling_after(const const_iterator place, Ts&&... args)
        {
            const size_t free_node = get_free_node();
            node& ref = data_[free_node];
            try { ref.value.emplace(std::forward<Ts>(args)...); }
            catch (...) // Failed to emplace, re-free the node
            {
                ref.value.next_free = std::exchange(free_list_, free_node);
                throw;
            }
            node& place_ref = data_[place.index_];
            ref.first_child = null_index;
            ref.parent = place_ref.parent;
            ref.next_sibling = std::exchange(place_ref.next_sibling, free_node);
            ++size_;
            return { this, free_node };
        }
    };
}
