#include <cassert>
#include <iostream>
#include <string>
#include <clu/flat_forest.h>

int main()
{
    clu::flat_forest<std::string> f;
    const auto root = f.emplace_child(f.end(), "Hello!");
    assert(root.is_root());
    const auto child = f.emplace_child(root, "Hi!");
    const auto next_child = f.emplace_sibling_after(child, "Howdy!");
    assert(child.next_sibling() == next_child);
    for (const auto& str : f) std::cout << str << ' '; // Hello! Hi! Howdy!
    std::cout << '\n';
    const auto new_first = f.emplace_child(root, "Greetings!");
    assert(new_first.next_sibling() == child);
    f.reset_parent(child, next_child);
    assert(next_child.first_child() == child);
    for (const auto& str : f) std::cout << str << ' '; // Hello! Greetings! Howdy! Hi!
}
