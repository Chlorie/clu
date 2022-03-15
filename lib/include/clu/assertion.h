#pragma once

#include <cstdio>
#include <stdexcept>

#include "macros.h"

#ifdef NDEBUG

#define CLU_ASSERT(expr, msg) ((void)0)

namespace clu
{
    inline constexpr bool is_debug = false;

    [[noreturn]] inline void unreachable()
    {
#if defined(CLU_GCC_COMPILERS)
        __builtin_unreachable();
#elif defined(CLU_MSVC_COMPILERS)
        __assume(false);
#else
        // No known unreachable builtin is available, just abort
        std::abort();
#endif
    }
} // namespace clu

#else

namespace clu
{
    inline constexpr bool is_debug = true;

    namespace detail
    {
        [[noreturn]] inline void assertion_failure(
            const char* expr, const char* msg, const char* file, const size_t line)
        {
            std::fprintf(stderr, "Assertion %s failed in %s, line %zu: %s", expr, file, line, msg);
            std::abort();
        }
    } // namespace detail

    [[noreturn]] inline void unreachable()
    {
        std::fprintf(stderr, "Supposedly unreachable code reached");
        std::abort();
    }
} // namespace clu

#define CLU_ASSERT(expr, msg) (void)(!!(expr) || (::clu::detail::assertion_failure(#expr, msg, __FILE__, __LINE__), 0))

#endif
