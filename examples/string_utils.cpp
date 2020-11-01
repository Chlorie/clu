#include <iostream>
#include <clu/string_utils.h>

int main()
{
    using namespace std::literals;
    constexpr size_t str_array_len = clu::strlen("hello"); // constexpr enabled
    constexpr size_t sv_len = clu::strlen("world"sv);
    const size_t cpp_str_len = clu::strlen("string"s);
    std::cout << "Lengths: " << str_array_len << ", " << sv_len << ", " << cpp_str_len << '\n';
}
