#pragma once

// Compiler related

#ifdef __GNUC__
#define CLU_GCC
#endif

#ifdef _MSC_VER
#define CLU_MSVC
#endif

#ifdef __clang__
#define CLU_CLANG
#endif

#ifdef CLU_GCC
#define CLU_GCC_WNO_OLD_STYLE_CAST _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")
#define CLU_GCC_WNO_CAST_FUNCTION_TYPE                                                                                 \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#define CLU_GCC_WNO_TSAN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wtsan\"")
#define CLU_GCC_RESTORE_WARNING _Pragma("GCC diagnostic pop")
#else
#define CLU_GCC_WNO_OLD_STYLE_CAST
#define CLU_GCC_WNO_CAST_FUNCTION_TYPE
#define CLU_GCC_WNO_TSAN
#define CLU_GCC_RESTORE_WARNING
#endif

// C++ language/library support check

#if __has_cpp_attribute(no_unique_address)
#define CLU_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define CLU_NO_UNIQUE_ADDRESS
#endif

#if __cplusplus >= 202302L && __cpp_static_call_operator >= 202207L
#define CLU_STATIC_CALL_OPERATOR_ARGS_(...) (__VA_ARGS__)
#define CLU_STATIC_CALL_OPERATOR(...) static __VA_ARGS__ operator() CLU_STATIC_CALL_OPERATOR_ARGS_
#else
#define CLU_STATIC_CALL_OPERATOR_ARGS_(...) (__VA_ARGS__) const
#define CLU_STATIC_CALL_OPERATOR(...) __VA_ARGS__ operator() CLU_STATIC_CALL_OPERATOR_ARGS_
#endif

#if __cplusplus >= 202302L && __cpp_if_consteval >= 202106L
#define CLU_IF_CONSTEVAL if consteval
#else
#define CLU_IF_CONSTEVAL if (std::is_constant_evaluated())
#endif

// Boilerplate generators

// clang-format off
#define CLU_SINGLE_RETURN_TRAILING(...)                                                                                \
    noexcept(noexcept(__VA_ARGS__)) -> decltype(__VA_ARGS__) { return __VA_ARGS__; }                                   \
    static_assert(true)

#define CLU_SINGLE_RETURN(...)                                                                                         \
    noexcept(noexcept(__VA_ARGS__)) { return __VA_ARGS__; }                                                            \
    static_assert(true)
// clang-format on

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
    type& operator=(type&&) = default
