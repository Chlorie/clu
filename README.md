# Chlorie's utilities library

Chlorie's collection of header-only small utilities. Requires C++20, will modularize as soon as major compilers & cmake supports standard modules.

## Contents

Detailed compilable examples are in the examples subdirectory, turn on CMake option `CLU_BUILD_EXAMPLES` to build the examples. 

All API in this library lies in namespace `clu`. The nested namespace `clu::detail` is for implementation details.

### Miscellaneous Utilities

- `concepts.h`: Common concepts like `similar_to` (`same_as` with `remove_cvref`) and so on.
- `debug.h`: Provides utilities for debugging purpose. Currently contains a `verbose` class that logs all of its special member function call into `stdout`.
- `enumerate.h`: Python-like enumerate function template for using range-`for` loops to iterate containers together with indices.
- `file.h`: Provides file utilities. Currently only contains a function template for reading the whole content of a binary file into a `std::vector`.
- `flat_forest.h`: An STL-like implementation for the forest data structure. Nodes are saved mostly contiguously inside memory. Provides fast structure modification methods (e.g. detach a branch and attach it elsewhere).
- `function_ref.h`: Non-owning type erasure class for invocables with a specific signature.
- `hash.h`: Simple constexpr string hashing function. Currently only contains the fnv1a-32 algorithm. Can be used for `switch`-`case` of strings.
- `indices.h`: Provides better syntax for writing normal indexed `for` loops.
- `optional_ref.h`: Optional reference type. Has rebind assignment semantics.
- `outcome.h`: Provides a wrapper around a value/null/exception union.
- `overload.h`: Helper class template for creating overloads of multiple lambdas, useful for visiting variants.
- `partial.h`: A simple library implementation to simulate the "pizza operator (`operator |>`)". Turns nested function calls to linear pipelines.
- `polymorphic_visit.h`: Provides a function template for achieving `std::visit` like functionality on a closed set of inheritance tree.
- `reference.h`: Utilities for dealing with `std::reference_wrapper`.
- `scope.h`: RAII wrapper for manual object lifetime management.
- `string_utils.h`: Some string utility functions.
- `take.h`: Provides utility function template for moving an object and reset it to the default constructed state.
- `type_traits.h`: Useful type traits.
    - `copy_cvref<From, To>`: Copies cv-qualifiers and references from the first type to the second. E.g. `copy_cvref<const int&, float>::type` gets `const float&`.
    - `all_same<Ts...>`: Checks whether a list of types are all the same.
- `vector_utils.h`: Contains a helper function template for constructing a vector with given list of elements, circumventing the non-movable problem of `std::initializer_list`.

### Coroutine related

APIs in this category lies under the `clu/coroutine` directory.

- `concepts.h`: Concepts for `awaiter`s, `awaitable`s, awaitable type traits, and more.
- `coroutine_scope.h`: A scope for launching coroutines in normal non-coroutine context.
- `generator.h`: A coroutine generator type which supports `co_yield`ing values in the coroutine body. `generator`s are also `std::input_range`s.
- `oneway_task.h`: An coroutine type supporting `co_await`ing in its body, `std::terminate`s if an uncaught exception escapes the coroutine body. The oneway-ness of this type means that this type is not an `awaitable`. This kind of coroutine also starts eagerly without suspending on calling them. This type is helpful for implementing other coroutine types, but not suitable for normal uses. For normal structural concurrent `co_await` uses, please use `task`.
- `schedule.h`: Specifies a `scheduler` concept and a corresponding `schedule` function template for scheduling tasks on a scheduler. Currently not used in other parts of this library.
- `sync_wait.h`: Synchronously waits for an `awaitable` to finish running. Can be used to start coroutines in normal functions with a blocking manner.
- `task.h`: A coroutine type for `co_await`ing `awaitable`s in the coroutine body. This task type is lazy, meaning that the coroutine body will not start just by constructing a `task`, instead you should `co_await` a `task` for that. This is an important building block of post-C++20 structural concurrency.
- `unique_coroutine_handle.h`: A unique ownership `std::coroutine_handle<T>` wrapper.
- `when_all_ready.h`: Constructs an awaitable for awaiting multiple awaitables concurrently. The results and potential exceptions are collected into `std::tuple` or `std::vector` of `outcome`s.

### Meta-programming related

APIs in this category lies under the `clu/meta` directory with namespace `clu::meta`.

- `type_list.h`: A "type list" struct template for "saving" a list of types.
    - `type_list<Ts...>`: The type list type.
    - `indexed_type<I, T>`: A "pair" of an index (`size_t`) and a type.
    - `list_size<L>`: A meta-function for getting the size of a type list.
    - `extract_list<T>`: For extracting type parameters from a type template, e.g. `extract_list<std::tuple<int, float>>::type` gets you `type_list<int, float>`.
    - `apply<L, Templ>`: Applies a type template back to a type list, e.g. `apply<type_list<int, float>, std::tuple>::type` gets back `std::tuple<int, float>`.
    - `front<L>`: Gets the first type in a type list.
    - `concatenate<L, M>`: Concatenates two type lists.
    - `find<L, T>`: This meta-function finds the index of the first occurrence of a type in a type list. If the type is not in the list, it returns `npos`.
    - `at<L, N>`: Gets a type from a type list given an index.


