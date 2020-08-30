#pragma once

#include <vector>

namespace clu
{
    /**
     * \brief A tree structure based on std::vector
     * \tparam T Value type
     * \remarks Currently not bothered with lifetime of the T
     */
    template <typename T>
    class flat_tree final
    {
    public:
        using value_type = T;

        /**
         * \brief A node in a flat tree structure
         * \remarks Invalidated after the flat tree moves
         */
        class node final
        {
        public:
            friend class flat_tree<T>;
            using value_type = T;

        private:
            static constexpr size_t null_index = static_cast<size_t>(-1);

            T value_;
            flat_tree<T>* tree_ = nullptr;
            size_t parent_ = null_index, child_ = null_index, next_sibling_ = null_index;

            node& tree_node(const size_t index) const { return tree_->nodes_[index]; }
            size_t index() const { return static_cast<size_t>(this - tree_->nodes_.data()); }

            void detach_from_parent()
            {
                node& p = parent();
                if (&p.first_child() == this)
                {
                    p.child_ = next_sibling_;
                    return;
                }
                node* c = &p.first_child();
                while (&c->next_sibling() != this) c = &c->next_sibling();
                c->next_sibling_ = next_sibling_;
            }

            void add_children_to_free_list(node*& list)
            {
                if (is_leaf()) return;
                size_t child = child_;
                while (child != null_index)
                {
                    list->parent_ = child;
                    node* ptr = &tree_node(child);
                    list = ptr;
                    ptr->add_children_to_free_list(list);
                    child = ptr->next_sibling_;
                }
            }

        public:
            template <typename... Ts>
            explicit node(flat_tree<T>* tree, Ts&&... args): value_(std::forward<Ts>(args)...), tree_(tree) {}

            flat_tree<T>& tree() const { return *tree_; }

            bool is_root() const { return parent_ == null_index; }
            bool is_leaf() const { return child_ == null_index; }
            bool is_last_child_of_parent() const { return next_sibling_ == null_index; }

            node& parent() const { return tree_node(parent_); }
            node& first_child() const { return tree_node(child_); }
            node& next_sibling() const { return tree_node(next_sibling_); }

            T& get() { return value_; }
            const T& get() const { return value_; }
            T& operator*() { return value_; }
            const T& operator*() const { return value_; }
            T* operator->() { return &value_; }
            const T* operator->() const { return &value_; }

            /**
             * \brief Construct a node using the parameters and add it to this node's children 
             * \return New reference to this node and the reference to the added child
             */
            template <typename... Ts>
            auto add_child(Ts&&... args)
            {
                struct result_t final
                {
                    node& parent;
                    node& child;
                };

                // this pointer may be invalidated, save variables into local scope
                const size_t this_index = index();
                flat_tree<T>& this_tree = tree();
                node& child = tree().emplace_into_free_node(std::forward<Ts>(args)...);
                node& parent = this_tree.nodes_[this_index]; // Get new this
                child.parent_ = this_index;
                child.child_ = null_index;
                child.next_sibling_ = null_index;
                const size_t ind = child.index();
                if (parent.is_leaf())
                    parent.child_ = ind;
                else
                {
                    node* last = &parent.first_child();
                    while (!last->is_last_child_of_parent()) last = &last->next_sibling();
                    last->next_sibling_ = ind;
                }
                return result_t{ parent, child };
            }

        };
        friend class node;

        /**
         * \brief Range class for pre-order traversal of the tree
         * \tparam IsConst Whether the iterators deference to constant references
         */
        template <bool IsConst>
        class preorder_range final
        {
        public:
            class iterator final
            {
            public:
                using difference_type = ptrdiff_t;
                using value_type = std::conditional_t<IsConst, const node, node>;
                using pointer = value_type*;
                using reference = value_type&;
                using iterator_category = std::forward_iterator_tag;

            private:
                pointer ptr_ = nullptr;

            public:
                iterator() = default;
                explicit iterator(const pointer ptr): ptr_(ptr) {}

                friend bool operator==(const iterator lhs, const iterator rhs) { return lhs.ptr_ == rhs.ptr_; }
                friend bool operator!=(const iterator lhs, const iterator rhs) { return lhs.ptr_ != rhs.ptr_; }

                reference operator*() const { return *ptr_; }
                pointer operator->() const { return ptr_; }

                // Non-recursive pre-order traversal
                iterator& operator++()
                {
                    if (!ptr_->is_leaf()) // Traverse children
                        ptr_ = &ptr_->first_child();
                    else if (!ptr_->is_last_child_of_parent()) // Next sibling
                        ptr_ = &ptr_->next_sibling();
                    else // Leaf node without next sibling
                    {
                        // Find first ancestor with more siblings
                        pointer ancestor = ptr_;
                        while (ancestor->is_last_child_of_parent())
                        {
                            if (ancestor->is_root()) // Last node to traverse
                            {
                                ptr_ = nullptr;
                                return *this;
                            }
                            ancestor = &ancestor->parent();
                        }
                        ptr_ = &ancestor->next_sibling();
                    }
                    return *this;
                }

                iterator operator++(int)
                {
                    iterator result = *this;
                    operator++();
                    return result;
                }
            };

            using const_iterator = iterator;

        private:
            using tree_type = std::conditional_t<IsConst, const flat_tree, flat_tree>;
            tree_type* ptr_ = nullptr;

        public:
            explicit preorder_range(tree_type* tree): ptr_(tree) {}

            iterator begin() const { return iterator(&ptr_->root()); }
            iterator end() const { return {}; }
        };

    private:
        static constexpr size_t null_index = static_cast<size_t>(-1);

        std::vector<node> nodes_{ node(this) };
        size_t root_ = 0;
        size_t free_ = null_index;

        template <typename... Ts>
        node& emplace_into_free_node(Ts&&... args)
        {
            if (free_ == null_index) // Should expand
                return nodes_.emplace_back(this, std::forward<Ts>(args)...);
            node& node = nodes_[free_];
            free_ = node.parent_;
            node.value_ = T(std::forward<Ts>(args)...);
            return node;
        }

        void prune_no_check(node& branch)
        {
            const size_t index = branch.index();
            if (free_ == null_index)
                free_ = index;
            else
                nodes_[free_].parent_ = index; // Use parent_ to indicate the next free node
            node* previous_free = &branch;
            branch.add_children_to_free_list(previous_free);
            previous_free->parent_ = null_index; // Last free node
        }

    public:
        const std::vector<node>& nodes() const { return nodes_; }
        node& root() { return nodes_[root_]; }
        const node& root() const { return nodes_[root_]; }

        auto preorder_traverser() { return preorder_range<false>(this); }
        auto preorder_traverser() const { return preorder_range<true>(this); }

        size_t capacity() const { return nodes_.capacity(); }

        void prune(node& branch)
        {
            if (branch.is_root())
                throw std::runtime_error("Can't prune root node");
            if (&branch.tree() != this)
                throw std::runtime_error("The branch to prune isn't from this tree");
            prune_no_check(branch);
        }

        void set_as_root(node& new_root)
        {
            if (&new_root.tree() != this)
                throw std::runtime_error("The new root isn't from this tree");
            if (root_ == new_root.index()) return;
            const size_t old_root = std::exchange(root_, new_root.index());
            new_root.detach_from_parent();
            prune_no_check(nodes_[old_root]);
        }
    };
}
