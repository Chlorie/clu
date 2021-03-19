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

int main()
{
    std::vector vec{ 1, 2, 3, 42, 2147483647 };
    print_buffer(vec);
}
