Header ``file.h``
===================

.. include:: ../roles.rst

This header provides helper functions to read/write data from/to files easily.

Examples
--------

The following code snippet reads a file and computes SHA-1 hash of the contents:

.. code-block:: cpp
    :linenos:

    #include <clu/file.h>
    #include <clu/assertion.h>

    int main()
    {
        std::vector<int> data{ 1, 2, 3, 4 };
        clu::write_all_bytes("data.bin", data);
        const auto read = clu::read_all_bytes<int>("data.bin"); // read as vector of ints
        const auto bytes = clu::read_all_bytes("data.bin"); // default behavior is to read as vector of bytes
        CLU_ASSERT(read == data);

        clu::write_all_text("data.txt", "Hello, world!");
        const std::string text = clu::read_all_text("data.txt");
        CLU_ASSERT(text == "Hello, world!");
    }

.. doxygenfunction:: read_all_bytes
.. doxygenfunction:: read_all_text
.. doxygenfunction:: write_all_bytes
.. doxygenfunction:: write_all_text
