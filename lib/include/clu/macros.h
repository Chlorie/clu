#pragma once

#ifdef __GNUC__
#   define CLU_GCC_COMPILERS
#elif defined(_MSC_VER)
#   define CLU_MSVC_COMPILERS
#endif

#ifdef CLU_GCC_COMPILERS
#   define CLU_GCC_WNO_CAST_FUNCTION_TYPE \
    _Pragma("GCC diagnostic push")        \
    _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#   define CLU_GCC_RESTORE_WARNING _Pragma("GCC diagnostic pop")
#else
#   define CLU_GCC_WNO_CAST_FUNCTION_TYPE
#   define CLU_GCC_RESTORE_WARNING
#endif
