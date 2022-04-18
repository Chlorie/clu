Header ``any_unique.h``
=======================

.. include:: ../roles.rst

Provides a type-erasure class to store objects of unknown types.

Examples
--------

.. code-block:: cpp
    :linenos:

    #include <clu/any_unique.h>

    int main()
    {
        clu::any_unique a(1); // a holds an int
        a.emplace(std::in_place_type<double>, 3.14); // now a holds a double, and the previous int is destroyed
        a.reset(); // a holds nothing

        struct immovable
        {
            immovable() = default;
            immovable(immovable const&) = delete;
            immovable(immovable&&) = delete;
            immovable& operator=(immovable const&) = delete;
            immovable& operator=(immovable&&) = delete;
        };

        clu::any_unique b(std::in_place_type<immovable>); // immovable objects can also be stored, using in_place_type
    }

.. doxygenclass:: clu::any_unique
    :members:
