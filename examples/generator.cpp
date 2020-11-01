#include <clu/coroutine/generator.h>

clu::generator<int> ints()
{
    for (int i = 0;; i++)
        co_yield i;
}

bool is_prime(const int value)
{
    if (value < 2) return false;
    for (int i = 2; i * i <= value; i++)
        if (value % i == 0) return false;
    return true;
}

clu::generator<int> filter_prime(clu::generator<int> seq)
{
    for (const int i : seq)
        if (is_prime(i))
            co_yield i;
}

int main()
{
    for (const int i : filter_prime(ints()))
    {
        if (i > 100) break;
        std::printf("%d ", i);
    }
}
