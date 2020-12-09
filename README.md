# Chlorie's utilities library

Chlorie's collection of header-only small utilities. Requires C++20, will modularize as soon as major compilers & cmake supports standard modules.

## Contents

Detailed compilable examples are in the examples subdirectory, turn on CMake option `CLU_BUILD_EXAMPLES` to build the examples. 

All API in this library lies in namespace `clu`. The nested namespace `clu::detail` is for implementation details.

### Miscellaneous Utilities

- `c_str_view.h`: A view type for a null-terminated C-style string.
- `concepts.h`: Common concepts like `similar_to` (`same_as` with `remove_cvref`) and so on.
- `debug.h`: Provides utilities for debugging purpose. Currently contains a `verbose` class that logs all of its special member function call into `stdout`.
- `enumerate.h`: Python-like enumerate function template for using range-`for` loops to iterate containers together with indices.
- `file.h`: Provides file utilities. Currently only contains a function template for reading the whole content of a binary file into a `std::vector`.
- `fixed_string.h`: A fixed-size string suitable for NTTP usage.
- `flat_forest.h`: An STL-like implementation for the forest data structure. Nodes are saved mostly contiguously inside memory. Provides fast structure modification methods (e.g. detach a branch and attach it elsewhere).
- `function_ref.h`: Non-owning type erasure class for invocables with a specific signature.
- `hash.h`: Simple constexpr string hashing function. Currently contains the fnv1a algorithm (`constexpr` enabled) and a hash combiner.
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

- `async_mutex.h`: An async mutex implementation. When a coroutine holding the mutex releases the lock, if there're more waiters on the mutex, the lock holder will resume the first waiter coroutine in the `unlock` function.
- `cancellable_task.h`: A task type similar to `task` but supports cancellation. Cancelling a `cancellable_task` would cancel any `cancellable_awaitable` that the task is awaiting on.
- `concepts.h`: Concepts for `awaiter`s, `awaitable`s, awaitable type traits, and more.
- `coroutine_scope.h`: A scope for launching coroutines in normal non-coroutine context.
- `fmap.h`: Create an awaitable that awaits an awaitable and transforms its result.
- `generator.h`: A coroutine generator type which supports `co_yield`ing values in the coroutine body. `generator`s are also `std::input_range`s.
- `oneway_task.h`: An coroutine type supporting `co_await`ing in its body, `std::terminate`s if an uncaught exception escapes the coroutine body. The oneway-ness of this type means that this type is not an `awaitable`. This kind of coroutine also starts eagerly without suspending on calling them. This type is helpful for implementing other coroutine types, but not suitable for normal uses. For normal structural concurrent `co_await` uses, please use `task`.
- `race.h`: Concurrently wait multiple cancellable awaitables. Once any awaitable finishes waiting (by returning or throwing exceptions) the other awaitables will be cancelled, and the result (or exception) of the winner awaitable will be returned.
- `sync_wait.h`: Synchronously waits for an `awaitable` to finish running. Can be used to start coroutines in normal functions with a blocking manner.
- `task.h`: A coroutine type for `co_await`ing `awaitable`s in the coroutine body. This task type is lazy, meaning that the coroutine body will not start just by constructing a `task`, instead you should `co_await` a `task` for that. This is an important building block of post-C++20 structural concurrency.
- `unique_coroutine_handle.h`: A unique ownership `std::coroutine_handle<T>` wrapper.
- `when_all.h`: Constructs an awaitable for awaiting multiple awaitables concurrently. The results are collected into `std::tuple` or `std::vector`. If any of the awaitables throws, the exception is propagated out. If you want to get the results of all awaitables even when some of them throws, or want to collect all the exceptions, please use `when_all_ready`.
- `when_all_ready.h`: Constructs an awaitable for awaiting multiple awaitables concurrently. The results and potential exceptions are collected into `std::tuple` or `std::vector` of `outcome`s.

### Meta-programming related

APIs in this category lies under the `clu/meta` directory with namespace `clu::meta`.

- `algorithm.h`: Algorithms for dealing with type lists.
    - `reduce<List, BinaryFunc, Init>`: Reduce a list given an initial value and a binary function.
    - `transform<List, UnaryFunc>`: Transform every type in a list with a function.
    - `all_of<List, Pred>`, `any_of<List, Pred>`, `none_of<List, Pred>`: Check if all/any/none of the types in a list satisfy a predicate.
    - `count<List, T>`, `count_if<List, Pred>`: Count the types in a list that is the same to a given type / satisfy a predicate.

- `functional.h`: Provides some useful meta-functions. Higher-order meta-functions are "called" with the nested alias template `call`.
    - `integral_constant`: Easier way to create a `std::integral_constant`.
    - `is_integral_constant`: Checks if a type is a `std::integral_constant`.
    - `bind_front<Func, Ts...>`, `bind_back<Func, Ts...>`: Binds parameters to a meta-function.
    - `plus`, `minus`, `multiplies`, `divides`, `modulus`, `negate`: Arithmetic operators.
    - `equal_to`, `not_equal_to`, `less`, `greater`, `less_equal`, `greater_equal`: Comparison operators.
    - `logical_and`, `logical_or`, `logical_not`: Logical operators.
    - `bit_and`, `bit_or`, `bit_xor`, `bit_not`: Bit-wise operators.
    - `not_fn<UnaryFunc>`: Inverts a predicate.

- `type_list.h`: A "type list" struct template for "saving" a list of types.
    - `type_list<Ts...>`: The type list type.
    - `indexed_type<I, T>`: A "pair" of an index (`size_t`) and a type.
    - `list_size<L>`: A meta-function for getting the size of a type list.
    - `to_type_list<S>`: Convert a `std::integer_sequence` into a `type_list`.
    - `to_integer_sequence<L>`: Convert a `type_list` into a `std::integer_sequence`.
    - `extract_list<T>`: For extracting type parameters from a type template, e.g. `extract_list<std::tuple<int, float>>::type` gets you `type_list<int, float>`.
    - `apply<L, Templ>`: Applies a type template back to a type list, e.g. `apply<type_list<int, float>, std::tuple>::type` gets back `std::tuple<int, float>`.
    - `front<L>`: Gets the first type in a type list.
    - `tail<L>`: Gets all the types except for the first one in a type list. 
    - `concatenate<L, M>`: Concatenates two type lists.
    - `find<L, T>`: This meta-function finds the index of the first occurrence of a type in a type list. If the type is not in the list, it returns `npos`.
    - `at<L, N>`: Gets a type from a type list given an index.


