#include <clu/scope.h>

void func(const bool do_throw)
{
    std::puts("Called!");
    clu::scope_exit guard([] { std::puts("Scoped exited!"); });
    if (do_throw) throw std::exception("Exception");
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
