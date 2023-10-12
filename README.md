# Chlorie's utilities library

[![Windows](https://github.com/Chlorie/clu/actions/workflows/windows-cl.yml/badge.svg)](https://github.com/Chlorie/clu/actions/workflows/windows-cl.yml)
[![Ubuntu](https://github.com/Chlorie/clu/actions/workflows/ubuntu-gcc.yml/badge.svg)](https://github.com/Chlorie/clu/actions/workflows/ubuntu-gcc.yml)

Chlorie's C++ library of various little things.

This is just a personal hobby side-project. A large portion of this library is untested and undocumented. Please use at your own risk.

Documentation (largely unfinished): <https://clutils.readthedocs.io/en/latest/>

## Provided Features

- Small abstractions that might be useful in various fields, e.g. a URI class, a UUID class, some common hash functions (`constexpr`-enabled), easy random number generation etc.
- Metaprogramming utilities, e.g. `type_list`, common meta list manipulation algorithms like `concatenate`, `filter`, `foldl` and such.
- More type traits and concepts, many of which are exposition-only concepts included in the standard but not exposed as public API, e.g. `callable`, `boolean_testable`, `template_of`, `function_traits` and so on.
- Library utilities, like iterator interface mixin, pipe-able function object factory, manual object lifetime control, and the like.
- STL-style containers and algorithms that complements the standard library, e.g. `static_vector`, `forest`, `indices`, find and replace for `string`s.
- Rough implementations of proposals for future versions of C++, for instance `tag_invoke`, `generator`, `expected`, and the senders/receivers framework for asynchronous programming.

## Supported Configurations

Please use the latest compiler appropriate for your system. If you want to try this library out with older compilers, you are on your own.

- This library uses C++20 language and library features extensively, it is suggested that you should use a relatively recent compiler.
    - The library is currently automatically tested and built on Windows with the latest MSVC (currently 17.7), and Ubuntu with the latest gcc (currently 13.2). These two compilers fully support all the C++20 features the library depends on.
    - clang (with libstdc++) support on Linux is currently under consideration. Since libc++ is really lagging behind on C++20 and even C++17 features, supporting it would be such a hassle, so I won't bother for now.
- Since I don't have a Mac, testing such configurations is difficult for me. It's currently not on my list.
