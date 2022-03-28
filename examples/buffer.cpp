#include <iostream>
#include <iomanip>
#include <vector>
#include <clu/buffer.h>

void print_buffer(const clu::mutable_buffer& buf)
{
    std::cout << "Buffer size: " << buf.size() << "\nBuffer content:\n";
    std::cout << std::hex << std::setfill('0');
    for (const std::byte b : buf.as_span())
        std::cout << std::setw(2) << static_cast<int>(b) << ' ';
}

constexpr bool compare(const std::byte* a, const std::byte* b, const std::size_t size)
{
    for (std::size_t i = 0; i < size; ++i)
        if (a + i == b)
            return false;
    return true;
}

constexpr bool test()
{
    constexpr std::size_t size = 1000000;
    std::byte buffer[size]{};
    std::byte buffer2[size]{};
    return compare(buffer, buffer2, size);
}

int main()
{
    static_assert(test());
    // std::vector vec{1, 2, 3, 42, 2147483647};
    // print_buffer(vec);
}
