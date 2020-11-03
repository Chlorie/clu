#include <iostream>
#include <clu/outcome.h>

clu::outcome<float> my_sqrt(const float value)
{
    if (value >= 0.0f) return std::sqrt(value);
    return clu::make_exceptional_outcome<float>(std::runtime_error("invalid input"));
}

int main()
{
    try
    {
        std::cout << *my_sqrt(2) << '\n';
        std::cout << *my_sqrt(-1) << '\n';
    }
    catch (const std::exception& exc)
    {
        std::cerr << exc.what() << '\n';
    }
}
