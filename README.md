# Chlorie's utilities library

Chlorie's small utility classes, functions, and various silly things. Requires C\++17 to compile, and will update to C\++20 when major compilers support it. Maybe containing bugs.

## Contents

- `enumerate.h`: Provides a Python-like enumerate function template for using range-`for` loops to iterate containers together with indices.
- `flat_tree.h`: A tree structure implementation based on `std::vector`. Uses a free list under the hood. Currently it doesn't go well with objects with complicated c'tors or d'tors, so only trivial structs can be used here.
- `quantifier.h`: For shortening similar comparisons. E.g. `x == 1 || x == 2` can be re-written as `x == clu::any_of(1, 2)`.
