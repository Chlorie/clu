#include <cstdio>
#include <clu/partial.h>

int square(const int i) { return i * i; }
int add(const int x, const int y) { return x + y; }
int get_value() { return 1; }

int before1()
{
    return add(square(add(get_value(), 5)), 6);
}

int before2()
{
    const int value = get_value();
    const int added = add(value, 5);
    const int squared = square(added);
    return add(squared, 6);
}

// Simulates the proposed "operator |>"
int after()
{
    return get_value()
        | clu::partial(add, 5)
        | clu::partial(square)
        | clu::partial(add, 6);
}

int main()
{
    const int result = after();
    std::printf("result = %d\n", result);
}
