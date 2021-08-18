Header ``hash.h``
===================

.. include:: ../roles.rst

This header provides hash-related helper functions and common hash algorithms. The algorithms are :cc:`constexpr` enabled if possible.

.. note::
   Unless specified otherwise, if the digest type of a hasher is a :cc:`std::array` of unsigned integers (words), the most significant word always comes last (little endian). The individual words are in native byte order, which may not be little endian.

Examples
--------

The following code snippet reads a file and computes SHA-1 hash of the contents:

.. code-block:: cpp
    :linenos:
    :emphasize-lines: 9

    #include <iostream>
    #include <format>
    #include <clu/file.h>
    #include <clu/hash.h>

    int main()
    {
        const auto bytes = clu::read_all_bytes("my_file.txt");
        const auto hash = clu::sha1(bytes);
        std::cout << std::format("SHA-1: {:08x}{:08x}{:08x}{:08x}{:08x}\n",
            hash[0], hash[1], hash[2], hash[3], hash[4]);
    }

We cannot use strings in :cc:`switch`-es, but utilizing the :cc:`constexpr` hash algorithms we are able to imitate that feature. Hash conflicts of cases are also detected at compile time which is neat. Just don't use this on arbitrary user inputs, where weird *(read "malicious")* hash clashes may occur.

.. code-block:: cpp
    :linenos:

    #include <clu/hash.h>

    int main()
    {
        using namespace clu::literals;     // for enabling the UDLs
        switch (clu::fnv1a("second"))      // regular function call
        {
            case "first"_fnv1a: return 1;  // or use UDLs
            case "second"_fnv1a: return 2;
            case "third"_fnv1a: return 3;
        }
    }
