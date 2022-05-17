#include "clu/execution_contexts/timed_threads.h"

namespace clu
{
    namespace detail::tm_loop
    {
        void ops_rbtree::insert(ops_base* node) noexcept
        {
            // Reset node
            node->set_parent_and_color(nullptr, rbt_color::black);
            node->children[left] = node->children[right] = nullptr;

            // Empty tree case
            if (!root_)
            {
                root_ = minimum_ = node;
                return;
            }

            // Just insert
            using enum rbt_color;
            if (node->deadline < minimum_->deadline)
                minimum_ = node;
            auto [parent, direction] = find_insert_position(node);
            node->set_parent_and_color(parent, red);
            parent->children[direction] = node;

            // Resolve red violations, precondition: current node is red
            do
            {
                // Case 1: parent is black, completes
                if (parent->color() == black)
                    return;

                ops_base* grandparent = parent->parent();
                // Case 4: parent is the root and also red
                if (!grandparent)
                {
                    parent->recolor(black);
                    return;
                }

                direction = which_child(parent);
                ops_base* uncle = grandparent->children[other(direction)];
                // Case 5, 6: red parent, black uncle
                if (!uncle || uncle->color() == black)
                {
                    // Case 5: ops is an inner child of its grandparent
                    if (which_child(node) == other(direction))
                    {
                        rotate(parent, direction);
                        std::swap(node, parent); // Reset current node
                    } // Fallthrough to case 6

                    // Case 6: ops is an outer child of its grandparent
                    rotate(grandparent, other(direction));
                    parent->recolor(black);
                    grandparent->recolor(red);
                    return;
                }

                // Case 2: red parent and uncle
                parent->recolor(black);
                uncle->recolor(black);
                grandparent->recolor(red);
                node = grandparent; // 2 levels (1 black level) higher
            } while ((parent = node->parent()));
            // Case 3: ops is the red root, completes
        }

        void ops_rbtree::remove(ops_base* node) noexcept
        {
            if (node == minimum_)
                update_minimum();
            if (remove_simple_cases(node))
                return;

            // node is a black non-root leaf
            using enum rbt_color;
            std::size_t direction = which_child(node);
            node->this_from_parent() = nullptr;
            ops_base* parent = node->parent();
            do
            {
                ops_base* sibling = parent->children[other(direction)]; // non-NIL
                ops_base* close = sibling->children[direction]; // Close nephew
                ops_base* distant = sibling->children[other(direction)]; // Distant nephew

                // Case 3: red sibling, black parent and nephews
                if (sibling->color() == red)
                {
                    rotate(parent, direction);
                    parent->recolor(red);
                    sibling->recolor(black);
                    sibling = close;
                    distant = sibling->children[other(direction)];
                    close = sibling->children[direction];
                    // Fallthrough to case 4, 5, or 6
                }

                const auto case6 = [&]
                {
                    rotate(parent, direction);
                    sibling->recolor(parent->color());
                    parent->recolor(black);
                    distant->recolor(black);
                };
                const auto case5 = [&]
                {
                    rotate(sibling, other(direction));
                    sibling->recolor(red);
                    close->recolor(black);
                    distant = sibling;
                    sibling = close;
                    case6();
                };

                // clang-format off
                // Case 6: red distant nephew, black sibling
                if (distant && distant->color() == red) { case6(); return; }
                // Case 5: red close nephew, black sibling and distant nephew
                if (close && close->color() == red) { case5(); return; }
                // clang-format on

                // Case 4: red parent, black sibling and nephews
                if (parent->color() == red)
                {
                    sibling->recolor(red);
                    parent->recolor(black);
                    return;
                }

                // Case 1: all black
                sibling->recolor(red);
                node = parent;
                parent = node->parent();
                if (!parent) // Case 2: parent is NIL, we reached the root node and completed
                    return;
                direction = which_child(node);
            } while (true);
        }

        ops_base* ops_rbtree::rotate(ops_base* const node, const std::size_t dir) noexcept
        {
            ops_base* parent = node->parent();
            ops_base* new_parent = node->children[other(dir)];
            ops_base* child = new_parent->children[dir];
            node->children[other(dir)] = child;
            fix_parent_of_child(node, other(dir));
            new_parent->children[dir] = node;
            if (parent)
                parent->children[which_child(node)] = new_parent;
            else
                root_ = new_parent;
            node->reparent(new_parent);
            new_parent->reparent(parent);
            return new_parent;
        }

        std::pair<ops_base*, std::size_t> ops_rbtree::find_insert_position(const ops_base* node) const noexcept
        {
            ops_base* parent = nullptr;
            ops_base* current = root_;
            std::size_t direction = left;
            while (current)
            {
                parent = current;
                direction = (current->deadline <= node->deadline) ? right : left;
                current = current->children[direction];
            }
            return {parent, direction};
        }

        // Called when the minimum node is removed
        void ops_rbtree::update_minimum() noexcept
        {
            if (minimum_->children[right])
            {
                minimum_ = minimum_->children[right];
                while (minimum_->children[left])
                    minimum_ = minimum_->children[left];
            }
            else
                minimum_ = minimum_->parent();
        }

        bool ops_rbtree::remove_simple_cases(ops_base* node) noexcept
        {
            using enum rbt_color;
            if (!minimum_) // The only node
            {
                root_ = nullptr;
                return true;
            }
            if (node->children[left] && node->children[right]) // Two non-NIL children
                swap_with_successor(node);
            // Now node has at most one non-NIL child, which is red if it exists
            if (node->color() == red) // No child, just remove
            {
                node->this_from_parent() = nullptr;
                return true;
            }
            if (node->children[left] || node->children[right]) // Black node with one red child
            {
                ops_base* child = node->children[left] ? node->children[left] : node->children[right];
                if (node->parent())
                    node->this_from_parent() = child;
                else
                    root_ = child;
                child->set_parent_and_color(node->parent(), black);
                return true;
            }
            return false;
        }

        // Precondition: node has two non-NIL children
        void ops_rbtree::swap_with_successor(ops_base* node) noexcept
        {
            // Find successor
            ops_base* successor = node->children[right];
            while (successor->children[left])
                successor = successor->children[left];

            // Swap the two nodes' positions
            if (node->parent())
                node->this_from_parent() = successor;
            else
                root_ = successor;
            std::swap(node->children[left], successor->children[left]);
            fix_parent_of_child(successor, left);
            if (successor == node->children[right]) // Direct descendant
            {
                if (ops_base* child = (node->children[right] = successor->children[right]))
                    child->reparent(node);
                const auto suc_color = successor->color();
                successor->children[right] = node;
                successor->parent_and_color = node->parent_and_color;
                node->set_parent_and_color(successor, suc_color);
            }
            else
            {
                successor->this_from_parent() = node;
                std::swap(node->parent_and_color, successor->parent_and_color);
                std::swap(node->children[right], successor->children[right]);
                fix_parent_of_child(node, right);
                fix_parent_of_child(successor, right);
            }
        }
    } // namespace detail::tm_loop

    timer_loop::~timer_loop() noexcept
    {
        if (!tree_.empty() || !stopped_)
            std::terminate();
    }

    void timer_loop::run()
    {
        while (ops_base* task = dequeue())
            task->set();
    }

    void timer_loop::finish()
    {
        std::unique_lock lock(mutex_);
        stopped_ = true;
        cv_.notify_all();
    }

    void timer_loop::enqueue(ops_base& ops)
    {
        const auto insert = [&]
        {
            std::unique_lock lock(mutex_);
            tree_.insert(&ops);
            return &ops == tree_.minimum();
        };
        if (insert()) // ops is the new minimum
            cv_.notify_one();
    }

    void timer_loop::cancel(ops_base& ops) noexcept
    {
        {
            std::unique_lock lock(mutex_);
            tree_.remove(&ops);
            ops.deadline = clock::now();
            tree_.insert(&ops);
        }
        cv_.notify_one();
    }

    timer_loop::ops_base* timer_loop::dequeue()
    {
        std::unique_lock lock(mutex_);

        // Wait for some deadline to appear
        cv_.wait(lock, [this] { return !tree_.empty() || stopped_; });
        if (stopped_)
            return nullptr;

        clock::time_point deadline = tree_.minimum()->deadline;
        while (true)
        {
            // Wait until the deadline / a deadline update / a stop request
            cv_.wait_until(lock, deadline, //
                [&] { return tree_.minimum()->deadline < deadline || stopped_; });
            if (stopped_)
                return nullptr;
            deadline = tree_.minimum()->deadline;

            // Deadline reached, pop minimum
            if (deadline <= clock::now())
            {
                ops_base* minimum = tree_.minimum();
                tree_.remove(minimum);
                return minimum;
            }
        }
    }
} // namespace clu
