#include <cstdio>
#include <stdexcept>
#include <clu/scope.h>

void func(const bool do_throw)
{
    std::puts("Called!");
    clu::scope_exit guard([] { std::puts("Scoped exited!"); });
    if (do_throw) throw std::runtime_error("Exception");
}

int main()
{
    try
    {
        func(false);
        func(true);
    }
    catch (...) {}
}
