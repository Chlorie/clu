#pragma once

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
        [[noreturn]] void assertion_failure(const char* expr, const char* msg, const char* file, size_t line);
    } // namespace detail

    [[noreturn]] void unreachable();
} // namespace clu

    #define CLU_ASSERT(expr, msg)                                                                                      \
        (void)(!!(expr) || (::clu::detail::assertion_failure(#expr, msg, __FILE__, __LINE__), 0))

#endif
