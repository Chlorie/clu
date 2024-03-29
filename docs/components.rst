Components
==========

.. include:: roles.rst

.. |p0323| replace:: P0323R10: :cc:`std::expected`
.. _p0323: https://wg21.link/p0323r10

Most of the components are under the main include directory ``clu``. Some of the core headers (like ``type_traits.h``) are used by other ones, while most of the other headers aren't depended upon. Things under ``clu/experimental`` are highly unstable, some of them may not even compile.

A list of all the headers and their contents is presented as follows:

:doc:`components/any_unique`
    Provides a type-erasure class to store objects of unknown types.
``assertion.h``
    Defines a macro :cc:`CLU_ASSERT(expr, msg)`, which functions roughly the same as ``assert`` from ``<cassert>``, but takes an additional message parameter.
``buffer.h``
    A buffer class template, similar to :cc:`std::span` but more suitable for raw buffer manipulation.
:doc:`components/c_str_view`
    View types for null-terminated strings.
``chrono_utils.h``
    Utilities related to :cc:`std::chrono`. Providing a centralized namespace for all the constants in :cc:`std::chrono` like :cc:`std::chrono::January`. Also some helper functions.
``concepts.h``
    Useful concepts for meta-programming.
``expected.h``
    A sum type storing a value or an error ``Ok | Err``, like ``Result`` in Rust. A rough implementation of |p0323|_.
:doc:`components/file`
    Contains a function ``read_all_bytes`` that reads all contents of a binary file into a :cc:`std::vector`.
``fixed_string.h``
    A fixed string class that can be used as class non-type template parameters.
``flags.h``
    A wrapper for flag enumeration types.
``forest.h``
    An STL-styled hierarchical container type.
``function_ref.h``
    Non-owning type-erasure types for invocables.
``function_traits.h``
    Type traits for function types. Provides meta-functions for extracting parameter types or result type from function signatures.
:doc:`components/hash`
    Hash-related utilities. Provides :cc:`constexpr` implementations of FNV-1a and SHA1 hash functions, and a ``hash_combine`` function.
``indices.h``
    A random-access range for multi-dimensional indexing, so that you could use :cc:`for (const auto [i, j, k] : indices(3, 4, 5))` instead of nested :cc:`for` loops.
``integer_literals.h``
    UDLs for fixed-length integral literals, like :cc:`255_u16`.
``iterator.h``
    Adapter class template for iterators. Provides default implementations for the iterator concepts.
``manual_lifetime.h``
    Helper class for manually controlling lifetime of an object.
``meta_algorithm.h``
    Algorithms for type lists.
``meta_list.h``
    Type lists and value lists for meta-programming.
``new.h``
    Helpers for aligned heap allocation.
``oneway_task.h``
    A fire-and-forget coroutine type which starts eagerly.
``optional_ref.h``
    :cc:`std::optional` like interface for optional references.
``overload.h``
    Overload set type. Useful for :cc:`std::visit`.
``scope.h``
    Scope guards for running arbitrary code at destruction. Useful for ad-hoc RAII constructs.
``static_for.h``
    Loop over compile time indices. Can also be used as a way to force loop unrolling.
``static_vector.h``
    :cc:`std::vector` like container allocated on the stack, with a fixed capacity.
``string_utils.h``
    String-related utilities.
``type_traits.h``
    Useful type traits for meta-programming.
``piper.h``
    "Pipeable" wrappers for invocables, like those ranges adapters.
``polymorphic_value.h``
    A smart pointer type for copyable polymorphic types. A rough implementation of `P0201R3: A polymorphic value-type for C++ <https://wg21.link/p0201r3>`_.
``polymorphic_visit.h``
    :cc:`std::visit`, but for polymorphic types.
``unique_coroutine_handle.h``
    An RAII type for :cc:`std::coroutine_handle<T>` which destroys the handle on destruction.
``vector_utils.h``
    A ``make_vector`` function to remedy the fact that we cannot use :cc:`std::initializer_list` to initialize containers with move-only elements.

.. toctree::
    :maxdepth: 1
    :glob:
    :hidden:
 
    components/*
