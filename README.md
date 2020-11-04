# Chlorie's utilities library

Chlorie's collection of header-only small utilities. Requires C++20, will modularize as soon as major compilers & cmake supports standard modules.

## Contents

Detailed compilable examples are in the examples subdirectory, turn on CMake option "CLU_BUILD_EXAMPLES" to build the examples. 

All API in this library lies in namespace `clu`. The nested namespace `clu::detail` is for implementation details.

- `concepts.h`: Common concepts like `similar_to` (`same_as` with `remove_cvref`) and so on.
- `enumerate.h`: Python-like enumerate function template for using range-`for` loops to iterate containers together with indices.
- `flat_forest.h`: An STL-like implementation for the forest data structure. Nodes are saved mostly contiguously inside memory. Provides fast structure modification methods (e.g. detach a branch and attach it elsewhere).
- `function_ref.h`: Non-owning type erasure class for invocables with a specific signature.
- `indices.h`: Provides better syntax for writing normal indexed `for` loops.
- `optional_ref.h`: Optional reference type. Has rebind assignment semantics.
- `outcome.h`: Provides a wrapper around a value/null/exception union.
- `overload.h`: Helper class template for creating overloads of multiple lambdas, useful for visiting variants.
- `partial.h`: A simple library implementation to simulate the "pizza operator (`operator |>`)". Turns nested function calls to linear pipelines.
- `scope.h`: RAII wrapper for manual object lifetime management.
- `string_utils.h`: Some string utility functions.

TODO: more descriptions and examples
