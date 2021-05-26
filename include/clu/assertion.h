#pragma once

#include <cstdio>
#include <stdexcept>

#ifdef NDEBUG

#define CLU_ASSERT(expression) ((void)0)

#else

namespace clu::detail
{
    inline void assertion_failure(const char* expr, const char* msg,
        const char* file, const size_t line)
    {
        std::fprintf(stderr, "Assertion %s failed in %s, line %zu: %s", expr, file, line, msg);
        std::terminate();
    }
}

#define CLU_ASSERT(expr, msg) \
    (void)(!!(expr) || (::clu::detail::assertion_failure(#expr, msg, __FILE__, __LINE__), 0))

#endif
