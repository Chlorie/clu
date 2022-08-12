# Chlorie's utilities library

Chlorie's C++ library of various little things.

This is just a personal hobby side-project. A large portion of this library is
untested and undocumented. Please use at your own risk.

Documentation (largely unfinished): https://clutils.readthedocs.io/en/latest/

## Provided Features

- Small abstractions that might be useful in various fields, e.g. a URI class,
a UUID class, some common hash functions (`constexpr`-enabled), easy random
number generation etc.
- Metaprogramming utilities, e.g. `type_list`, common meta list manipulation
algorithms like `concatenate`, `filter`, `foldl` and such.
- More type traits and concepts, many of which are exposition-only concepts
included in the standard but not exposed as public API, e.g. `callable`,
`boolean_testable`, `template_of`, `function_traits` and so on.
- Library utilities, like iterator interface mixin, pipe-able function object
factory, manual object lifetime control, and the like.
- STL-style containers and algorithms that complements the standard library,
e.g. `static_vector`, `forest`, `indices`, find and replace for `string`s.
- Rough implementations of proposals for future versions of C++, for instance
`tag_invoke`, `generator`, `expected`, and the senders/receivers framework
for asynchronous programming.

## Supported Compilers

Please use the latest versions. If you want to try this library out with older
compilers, please read the notes below.

- This library uses C++20 language and library features extensively, it is
suggested that you should use a relatively recent compiler.
- MSVC 14.32 (Visual Studio 2022 17.2) has some major issues with templates,
complex template meta-programming might not compile under this version.
Former versions (Visual Studio 2022 17.1) works fine.
- As per the time of writing, clang still does not support coroutines
officially, you should use libc++ and `<experimental/coroutine>` for
coroutine facilities. `clu` provides a `<clu/coroutine.h>` for this use case.
The `clu::coro` namespace alias refers to `std` or `std::experimental`
depending on your standand library. For Visual Studio users, clang-cl works
with the MSVC STL's coroutine implementation, so you can use that.
- As per the time of writing, neither gcc (libstdc++) nor clang (libc++)
implements C++20 string formatting (`<format>`) fully. So non-Windows support
is missing at this point of time.
