#pragma once

// Compiler related

#ifdef __GNUC__
#define CLU_GCC_COMPILERS
#elif defined(_MSC_VER)
#define CLU_MSVC_COMPILERS
#endif

#ifdef CLU_GCC_COMPILERS
#define CLU_GCC_WNO_OLD_STYLE_CAST _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")
#define CLU_GCC_WNO_CAST_FUNCTION_TYPE                                                                                 \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#define CLU_GCC_RESTORE_WARNING _Pragma("GCC diagnostic pop")
#else
#define CLU_GCC_WNO_OLD_STYLE_CAST
#define CLU_GCC_WNO_CAST_FUNCTION_TYPE
#define CLU_GCC_RESTORE_WARNING
#endif

// Boilerplate generators

#define CLU_SINGLE_RETURN(expr)                                                                                        \
    noexcept(noexcept(expr))->decltype(expr) { return expr; }                                                          \
    static_assert(true)

#define CLU_NON_COPYABLE_TYPE(type)                                                                                    \
    type(const type&) = delete;                                                                                        \
    type& operator=(const type&) = delete

#define CLU_IMMOVABLE_TYPE(type)                                                                                       \
    type(const type&) = delete;                                                                                        \
    type(type&&) = delete;                                                                                             \
    type& operator=(const type&) = delete;                                                                             \
    type& operator=(type&&) = delete

#define CLU_DEFAULT_MOVE_MEMBERS(type)                                                                                 \
    type(type&&) = default;                                                                                            \
    type& operator=(type&&) = default;                                                                                 \
    ~type() noexcept = default
