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

- `flat_tree.h`: A tree structure implementation based on `std::vector`. Uses a free list under the hood. Currently it doesn't go well with objects with complicated c'tors or d'tors, so only trivial structs can be used here.

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
