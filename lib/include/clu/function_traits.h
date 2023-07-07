#pragma once

#include "type_traits.h"
#include "meta_list.h"

namespace clu
{
    namespace detail
    {
        template <typename>
        struct function_traits_base
        {
        };

        template <typename R, typename... Ts>
        struct function_traits_base<R(Ts...)>
        {
            using return_type = R;
            using argument_types = type_list<Ts...>;
            static constexpr size_t arity = sizeof...(Ts);
        };
    } // namespace detail

    template <typename F>
    struct function_traits
    {
    };

#define CLU_FT_SPECIALIZE(cv, ref, noexc, noexc_bool)                                                                  \
    template <typename R, typename... Ts>                                                                              \
    struct function_traits<R(Ts...) cv ref noexc> : detail::function_traits_base<R(Ts...)>                             \
    {                                                                                                                  \
        static constexpr bool is_const = std::is_const_v<cv int>;                                                      \
        static constexpr bool is_volatile = std::is_volatile_v<cv int>;                                                \
        static constexpr bool is_lvalue_ref = std::is_lvalue_reference_v<int ref>;                                     \
        static constexpr bool is_rvalue_ref = std::is_rvalue_reference_v<int ref>;                                     \
        static constexpr bool is_noexcept = noexc_bool;                                                                \
        static constexpr bool is_vararg = false;                                                                       \
    };                                                                                                                 \
                                                                                                                       \
    template <typename R, typename... Ts>                                                                              \
    struct function_traits<R(Ts..., ...) cv ref noexc> : detail::function_traits_base<R(Ts...)>                        \
    {                                                                                                                  \
        static constexpr bool is_const = std::is_const_v<cv int>;                                                      \
        static constexpr bool is_volatile = std::is_volatile_v<cv int>;                                                \
        static constexpr bool is_lvalue_ref = std::is_lvalue_reference_v<int ref>;                                     \
        static constexpr bool is_rvalue_ref = std::is_rvalue_reference_v<int ref>;                                     \
        static constexpr bool is_noexcept = noexc_bool;                                                                \
        static constexpr bool is_vararg = true;                                                                        \
    }

    CLU_FT_SPECIALIZE(, , , false);
    CLU_FT_SPECIALIZE(const, , , false);
    CLU_FT_SPECIALIZE(volatile, , , false);
    CLU_FT_SPECIALIZE(const volatile, , , false);
    CLU_FT_SPECIALIZE(, &, , false);
    CLU_FT_SPECIALIZE(const, &, , false);
    CLU_FT_SPECIALIZE(volatile, &, , false);
    CLU_FT_SPECIALIZE(const volatile, &, , false);
    CLU_FT_SPECIALIZE(, &&, , false);
    CLU_FT_SPECIALIZE(const, &&, , false);
    CLU_FT_SPECIALIZE(volatile, &&, , false);
    CLU_FT_SPECIALIZE(const volatile, &&, , false);
    CLU_FT_SPECIALIZE(, , noexcept, true);
    CLU_FT_SPECIALIZE(const, , noexcept, true);
    CLU_FT_SPECIALIZE(volatile, , noexcept, true);
    CLU_FT_SPECIALIZE(const volatile, , noexcept, true);
    CLU_FT_SPECIALIZE(, &, noexcept, true);
    CLU_FT_SPECIALIZE(const, &, noexcept, true);
    CLU_FT_SPECIALIZE(volatile, &, noexcept, true);
    CLU_FT_SPECIALIZE(const volatile, &, noexcept, true);
    CLU_FT_SPECIALIZE(, &&, noexcept, true);
    CLU_FT_SPECIALIZE(const, &&, noexcept, true);
    CLU_FT_SPECIALIZE(volatile, &&, noexcept, true);
    CLU_FT_SPECIALIZE(const volatile, &&, noexcept, true);

#undef CLU_FT_SPECIALIZE

    namespace detail
    {
        template <typename C, typename F>
        constexpr auto implicit_param_type_impl()
        {
            using base = function_traits<F>;
            using added_const = std::conditional_t<base::is_const, const C, C>;
            using added_cv = std::conditional_t<base::is_volatile, volatile added_const, added_const>;
            using added_cvref = std::conditional_t<base::is_rvalue_ref, added_cv&&, added_cv&>;
            return type_tag<added_cvref>;
        }
    } // namespace detail

    template <typename C, typename F>
        requires std::is_function_v<F>
    struct function_traits<F C::*> : function_traits<F>
    {
        using class_type = C;
        using implicit_param_type = typename decltype(detail::implicit_param_type_impl<C, F>())::type;
    };
} // namespace clu
