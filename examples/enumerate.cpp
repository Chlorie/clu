#include <clu/experimental/enumerate.h>
#include <vector>
#include <string>
#include <iostream>

int main()
{
    const std::vector<std::string> strings
    {
        "Hello", "World", "Yeah",
        "Things", "Strings", "Vector"
    };
    for (const auto [index, str] : clu::enumerate(strings))
    {
        std::cout << "index: " << index
            << " string: " << str << '\n';
    }
}
