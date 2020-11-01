#include <cstdio>
#include <clu/indices.h>

int main()
{
    for (auto i : clu::indices(10))
        std::printf("i = %zu; ", i);
    std::puts("");
    for (auto [i, j, k] : clu::indices(2, 3, 4))
        std::printf("i, j, k = %zu, %zu, %zu\n", i, j, k);
}
