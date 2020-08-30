#pragma once

#include <vector>

namespace clu
{
    /**
     * \brief A tree structure based on std::vector
     * \tparam T Value type
     * \remarks Currently not bothered with lifetime of the T...
     */
    template <typename T>
    class flat_tree final
    {
    private:
        static constexpr size_t null_index = static_cast<size_t>(-1);

    public:
        using value_type = T;

        struct node final
        {
            T value_;
            flat_tree<T>* tree_ = nullptr;
            size_t parent_ = null_index, child_ = null_index, next_sibling_ = null_index;

            template <typename... Ts>
            explicit node(flat_tree<T>* tree, Ts&&... args): value_(std::forward<Ts>(args)...), tree_(tree) {}

            node& tree_node(const size_t index) const { return tree_->nodes_[index]; }
            size_t index() const { return static_cast<size_t>(this - tree_->nodes_.data()); }

            flat_tree<T>& tree() const { return *tree_; }

            bool is_root() const { return parent_ == null_index; }
            bool is_leaf() const { return child_ == null_index; }
            bool is_last_child_of_parent() const { return next_sibling_ == null_index; }

            node& parent() const { return tree_node(parent_); }
            node& first_child() const { return tree_node(child_); }
            node& next_sibling() const { return tree_node(next_sibling_); }

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
        };
        friend struct node;

        template <bool IsConst>
        class preorder_iterator final
        {
            friend class preorder_iterator<true>;
            friend class flat_tree;
        private:
            using qualified_value_type = std::conditional_t<IsConst, const T, T>;
            using qualified_tree_type = std::conditional_t<IsConst, const flat_tree, flat_tree>;

        public:
            using difference_type = ptrdiff_t;
            using value_type = T;
            using pointer = qualified_value_type*;
            using reference = qualified_value_type&;
            using iterator_category = std::forward_iterator_tag;

        private:
            qualified_tree_type* tree_ = nullptr;
            size_t index_ = null_index;

            auto&& ref() const { return tree_->nodes_[index_]; }

        public:
            preorder_iterator() = default;
            preorder_iterator(const preorder_iterator<false>& it): tree_(it.tree_), index_(it.index_) {}
            preorder_iterator(qualified_tree_type* const ptr, const size_t index): tree_(ptr), index_(index) {}
            explicit preorder_iterator(const node& n): tree_(n.tree_), index_(n.index()) {}

            friend bool operator==(const preorder_iterator lhs, const preorder_iterator rhs)
            {
                return lhs.tree_ == rhs.tree_ && lhs.index_ == rhs.index_;
            }
            friend bool operator!=(const preorder_iterator lhs, const preorder_iterator rhs)
            {
                return lhs.tree_ != rhs.tree_ || lhs.index_ != rhs.index_;
            }

            reference operator*() const { return ref().value_; }
            pointer operator->() const { return &ref().value_; }

            preorder_iterator& operator++()
            {
                const node* ptr = &ref();
                if (!ptr->is_leaf()) // Traverse children
                    index_ = ptr->child_;
                else if (!ptr->is_last_child_of_parent()) // Next sibling
                    index_ = ptr->next_sibling_;
                else // Leaf node without next sibling
                {
                    // Find first ancestor with more siblings
                    const node* ancestor = ptr;
                    while (ancestor->is_last_child_of_parent())
                    {
                        if (ancestor->is_root()) // Last node to traverse
                        {
                            index_ = null_index;
                            return *this;
                        }
                        ancestor = &ancestor->parent();
                    }
                    index_ = ancestor->next_sibling_;
                }
                return *this;
            }

            preorder_iterator operator++(int)
            {
                preorder_iterator result = *this;
                operator++();
                return result;
            }

            qualified_tree_type& tree() const { return *tree_; }

            bool is_root() const { return ref().is_root(); }
            bool is_leaf() const { return ref().is_leaf(); }
            bool is_last_child_of_parent() const { return ref().is_last_child_of_parent(); }

            preorder_iterator parent() const { return { tree_, ref().parent_ }; }
            preorder_iterator first_child() const { return { tree_, ref().child_ }; }
            preorder_iterator next_sibling() const { return { tree_, ref().next_sibling_ }; }
        };
        template <bool IsConst> friend class preorder_iterator;

        using iterator = preorder_iterator<false>;
        using const_iterator = preorder_iterator<true>;

    private:
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

        void prune_no_check(const size_t index)
        {
            node& branch = nodes_[index];
            if (free_ == null_index)
                free_ = index;
            else
                nodes_[free_].parent_ = index; // Use parent_ to indicate the next free node
            node* previous_free = &branch;
            branch.add_children_to_free_list(previous_free);
            previous_free->parent_ = null_index; // Last free node
        }

    public:
        template <typename... Ts>
        explicit flat_tree(Ts&&... root_args): nodes_{ node(this, std::forward<Ts>(root_args)...) } {}

        iterator root() { return { this, root_ }; }
        const_iterator root() const { return { this, root_ }; }

        iterator begin() { return root(); }
        const_iterator begin() const { return root(); }
        const_iterator cbegin() const { return root(); }
        iterator end() { return { this, null_index }; }
        const_iterator end() const { return { this, null_index }; }
        const_iterator cend() const { return { this, null_index }; }

        size_t capacity() const { return nodes_.capacity(); }

        /**
         * \brief Prune (remove) a sub-tree providing its root
         * \param branch An iterator to the branch
         * \remarks Root of the whole tree cannot be pruned.
         * Only iterators to the pruned nodes are invalidated after this action.
         */
        void prune(const const_iterator branch)
        {
            if (branch.is_root())
                throw std::runtime_error("Can't prune root node");
            if (&branch.tree() != this)
                throw std::runtime_error("The branch to prune isn't from this tree");
            nodes_[branch.index_].detach_from_parent();
            prune_no_check(branch.index_);
        }

        /**
         * \brief Replace current tree with a sub-tree of itself
         * \param new_root Iterator to the root node of the sub-tree
         * \remarks Only iterators to the removed nodes are invalidated after this action.
         */
        void set_as_root(const const_iterator new_root)
        {
            if (&new_root.tree() != this)
                throw std::runtime_error("The new root isn't from this tree");
            if (root_ == new_root.index_) return;
            const size_t old_root = std::exchange(root_, new_root.index_);
            nodes_[new_root.index_].detach_from_parent();
            prune_no_check(old_root);
        }

        /**
         * \brief Construct a new node using the parameters and add it as another node's children 
         * \return Iterator to the new child
         * \remark No iterator is invalidated after this action
         */
        template <typename... Ts>
        iterator add_child(const const_iterator iter, Ts&&... args)
        {
            node& child = emplace_into_free_node(std::forward<Ts>(args)...);
            node& parent = nodes_[iter.index_]; // Get new this
            child.parent_ = iter.index_;
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
            return iterator(this, ind);
        }
    };
}
