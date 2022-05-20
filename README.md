# Chlorie's utilities library

Chlorie's collection of small utilities for C++20.

Currently this is just for personal use, a large portion of this library is
untested and undocumented. Please use at your own risk.

Documentation: https://clutils.readthedocs.io/en/latest/

## Supported Compilers

Please use the latest versions unless noted below.

- MSVC 14.32 (Visual Studio 2022 17.2) has some major issues with templates,
complex template meta-programming might not compile under this version.
The former version (Visual Studio 2022 17.1) works fine.
- As per the time of writing, clang still does not support coroutines
officially, you should use libc++ and `<experimental/coroutine>` for
coroutine facilities. `clu` provides a `<clu/coroutine.h>` for this use case.
The `clu::coro` namespace alias refers to `std` or `std::experimental`
depending on your standand library. For Windows users, you can use clang-cl
together with the MSVC STL for all the C++20 features.
- As per the time of writing, neither gcc (libstdc++) nor clang (libc++)
implements C++20 string formatting (`<format>`) fully. So non-Windows support
is missing at this point of time.
