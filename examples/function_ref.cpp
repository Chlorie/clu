#include <clu/function_ref.h>

void call_thrice(const clu::function_ref<void()> fref)
{
    fref();
    fref();
    fref();
}

void c_func() { std::puts("c_func"); }

auto counter = [i = 0]() mutable
{
    std::printf("counting %d\n", i);
    i++;
};

int main()
{
    call_thrice(c_func);
    call_thrice(counter);
}
