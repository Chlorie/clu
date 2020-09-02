# Chlorie's utilities library

Chlorie's small utility classes, functions, and various silly things. Requires C\++17 to compile, and will update to C\++20 when major compilers support it. May contain bugs.

## Contents

All API in this library lies in namespace `clu`. The nested namespace `clu::detail` is for implementation details.

- `enumerate.h`: Provides a Python-like enumerate function template for using range-`for` loops to iterate containers together with indices.

    ```cpp
    size_t i = 0;
    for (auto&& elem : range)
    {
        // Do things with index "i" and element "elem"
        i++;
    }
    // Equivalent to
    for (auto [i, elem] : enumerate(range))
        // Do things with index "i" and element "elem"
    ```

- `flat_forest.h`: An STL-like implementation for the forest data structure. Nodes are saved mostly contiguously inside memory. Provides fast structure modification methods (e.g. detach a branch and attach it elsewhere).

    ```cpp
    flat_forest<std::string> f;
    const auto /*forest<std::string>::iterator*/ root = f.emplace_child(f.end(), "Hello!");
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
    ```

- `indices.h`: Provides better syntax for writing normal indexed `for` loops.

    ```cpp
    // Single loop
    for (size_t i = 0; i < 100; i++)
    // Equivalent to
    for (auto i : indices(100))
    
    // Nested loop:
    for (size_t i = 0; i < 50; i++)
        for (size_t j = 0; j < 25; j++)
    // Equivalent to
    for (const auto& [i, j] : indices(50, 25))
    ```

- `quantifier.h`: For making multiple similar comparisons more readable.

    ~~~~ cpp
    x == 1 || x == 2 || x == 3
    // Equivalent to
    x == any_of(1, 2, 3)
    ~~~~
