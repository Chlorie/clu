#include "clu/assertion.h"

#ifndef NDEBUG

namespace clu
{
    namespace detail
    {
        void assertion_failure(const char* expr, const char* msg, const char* file, const size_t line)
        {
            std::fprintf(stderr, "Assertion %s failed in %s, line %zu: %s", expr, file, line, msg);
            std::abort();
        }
    } // namespace detail
    void unreachable()
    {
        std::fprintf(stderr, "Supposedly unreachable code reached");
        std::abort();
    }
} // namespace clu

#endif
