#pragma once

#include "meta/type_list.h"

namespace clu
{
    namespace detail
    {
        template <typename> struct function_traits_base {};

        template <typename R, typename... Ts>
        struct function_traits_base<R(Ts ...)>
        {
            using return_type = R;
            using argument_types = meta::type_list<Ts...>;
            static constexpr size_t arity = sizeof...(Ts);
        };
    }

    template <typename> struct function_traits {};

    template <typename C, typename F> requires std::is_function_v<F>
    struct function_traits<F C::*> : function_traits<F>
    {
        using class_type = C;
    };

#define CLU_FT_SPECIALIZE(cv, ref, noexc, noexc_bool)                                        \
    template <typename R, typename... Ts>                                                    \
    struct function_traits<R(Ts ...) cv ref noexc> : detail::function_traits_base<R(Ts ...)> \
    {                                                                                        \
        static constexpr bool is_const = std::is_const_v<cv int>;                            \
        static constexpr bool is_volatile = std::is_volatile_v<cv int>;                      \
        static constexpr bool is_lvalue_ref = std::is_lvalue_reference_v<int ref>;           \
        static constexpr bool is_rvalue_ref = std::is_rvalue_reference_v<int ref>;           \
        static constexpr bool is_noexcept = noexc_bool;                                      \
        static constexpr bool is_vararg = false;                                             \
    }

#define CLU_FT_SPECIALIZE_VARARG(cv, ref, noexc, noexc_bool)                                      \
    template <typename R, typename... Ts>                                                         \
    struct function_traits<R(Ts ..., ...) cv ref noexc> : detail::function_traits_base<R(Ts ...)> \
    {                                                                                             \
        static constexpr bool is_const = std::is_const_v<cv int>;                                 \
        static constexpr bool is_volatile = std::is_volatile_v<cv int>;                           \
        static constexpr bool is_lvalue_ref = std::is_lvalue_reference_v<int ref>;                \
        static constexpr bool is_rvalue_ref = std::is_rvalue_reference_v<int ref>;                \
        static constexpr bool is_noexcept = noexc_bool;                                           \
        static constexpr bool is_vararg = true;                                                   \
    }

    // @formatter:off
    CLU_FT_SPECIALIZE(              ,   ,         , false);
    CLU_FT_SPECIALIZE(const         ,   ,         , false);
    CLU_FT_SPECIALIZE(volatile      ,   ,         , false);
    CLU_FT_SPECIALIZE(const volatile,   ,         , false);
    CLU_FT_SPECIALIZE(              , & ,         , false);
    CLU_FT_SPECIALIZE(const         , & ,         , false);
    CLU_FT_SPECIALIZE(volatile      , & ,         , false);
    CLU_FT_SPECIALIZE(const volatile, & ,         , false);
    CLU_FT_SPECIALIZE(              , &&,         , false);
    CLU_FT_SPECIALIZE(const         , &&,         , false);
    CLU_FT_SPECIALIZE(volatile      , &&,         , false);
    CLU_FT_SPECIALIZE(const volatile, &&,         , false);
    CLU_FT_SPECIALIZE(              ,   , noexcept, true );
    CLU_FT_SPECIALIZE(const         ,   , noexcept, true );
    CLU_FT_SPECIALIZE(volatile      ,   , noexcept, true );
    CLU_FT_SPECIALIZE(const volatile,   , noexcept, true );
    CLU_FT_SPECIALIZE(              , & , noexcept, true );
    CLU_FT_SPECIALIZE(const         , & , noexcept, true );
    CLU_FT_SPECIALIZE(volatile      , & , noexcept, true );
    CLU_FT_SPECIALIZE(const volatile, & , noexcept, true );
    CLU_FT_SPECIALIZE(              , &&, noexcept, true );
    CLU_FT_SPECIALIZE(const         , &&, noexcept, true );
    CLU_FT_SPECIALIZE(volatile      , &&, noexcept, true );
    CLU_FT_SPECIALIZE(const volatile, &&, noexcept, true );

    CLU_FT_SPECIALIZE_VARARG(              ,   ,         , false);
    CLU_FT_SPECIALIZE_VARARG(const         ,   ,         , false);
    CLU_FT_SPECIALIZE_VARARG(volatile      ,   ,         , false);
    CLU_FT_SPECIALIZE_VARARG(const volatile,   ,         , false);
    CLU_FT_SPECIALIZE_VARARG(              , & ,         , false);
    CLU_FT_SPECIALIZE_VARARG(const         , & ,         , false);
    CLU_FT_SPECIALIZE_VARARG(volatile      , & ,         , false);
    CLU_FT_SPECIALIZE_VARARG(const volatile, & ,         , false);
    CLU_FT_SPECIALIZE_VARARG(              , &&,         , false);
    CLU_FT_SPECIALIZE_VARARG(const         , &&,         , false);
    CLU_FT_SPECIALIZE_VARARG(volatile      , &&,         , false);
    CLU_FT_SPECIALIZE_VARARG(const volatile, &&,         , false);
    CLU_FT_SPECIALIZE_VARARG(              ,   , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(const         ,   , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(volatile      ,   , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(const volatile,   , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(              , & , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(const         , & , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(volatile      , & , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(const volatile, & , noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(              , &&, noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(const         , &&, noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(volatile      , &&, noexcept, true );
    CLU_FT_SPECIALIZE_VARARG(const volatile, &&, noexcept, true );
    // @formatter:on

#undef CLU_FT_SPECIALIZE
#undef CLU_FT_SPECIALIZE_VARARG
}
