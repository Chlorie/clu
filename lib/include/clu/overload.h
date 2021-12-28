// Rough implementation of P0051R3: C++ generic overload function

#pragma once

#include <functional>

#include "type_traits.h"
#include "invocable_wrapper.h"

namespace clu
{
    namespace detail
    {
        template <typename... Fs>
        struct overload_t : invocable_wrapper<Fs>...
        {
            using invocable_wrapper<Fs>::operator()...;
        };

        template <typename... Fs>
        class first_overload_t : public invocable_wrapper<Fs>...
        {
        private:
            struct not_callable_t {};

            template <typename FirstBase, typename... Bases, typename Self, typename... Args>
            constexpr static auto call_result_impl(Self&& self, Args&&... args) noexcept
            {
                using base = copy_cvref_t<Self&&, FirstBase>;
                if constexpr (std::is_invocable_v<base, Args&&...>)
                    return std::invoke_result<base, Args&&...>{};
                else if constexpr (sizeof...(Bases) > 0)
                    return call_result_impl<Bases...>(static_cast<Self&&>(self), static_cast<Args&&>(args)...);
                else
                    return not_callable_t{};
            }

            template <typename FirstBase, typename... Bases, typename Self, typename... Args>
            constexpr static auto is_nothrow_callable_impl(Self&& self, Args&&... args) noexcept
            {
                using base = copy_cvref_t<Self&&, FirstBase>;
                if constexpr (std::is_invocable_v<base, Args&&...>)
                    return std::is_nothrow_invocable<base, Args&&...>{};
                else if constexpr (sizeof...(Bases) > 0)
                    return is_nothrow_callable_impl<Bases...>(static_cast<Self&&>(self), static_cast<Args&&>(args)...);
                else
                    return std::false_type{};
            }

            template <typename Self, typename... Args>
            using call_result = decltype(call_result_impl<invocable_wrapper<Fs>...>(std::declval<Self>(), std::declval<Args&&>()...));
            template <typename Self, typename... Args>
            using call_result_t = typename call_result<Self, Args...>::type;
            template <typename Self, typename... Args>
            static constexpr bool is_callable_v = !std::is_same_v<not_callable_t, call_result<Self, Args...>>;
            template <typename Self, typename... Args>
            static constexpr bool is_nothrow_callable_v = decltype(is_nothrow_callable_impl<invocable_wrapper<Fs>...>(
                std::declval<Self>(), std::declval<Args&&>()...))::value;

            template <typename FirstBase, typename... Bases, typename Self, typename... Args>
            constexpr static decltype(auto) call_impl(Self&& self, Args&&... args) noexcept(is_nothrow_callable_v<Self, Args...>)
            {
                using base = copy_cvref_t<Self&&, FirstBase>;
                if constexpr (std::is_invocable_v<base, Args&&...>)
                    return static_cast<Self&&>(self).FirstBase::operator()(static_cast<Args&&>(args)...);
                else if constexpr (sizeof...(Bases) > 0)
                    return call_impl<Bases...>(static_cast<Self&&>(self), static_cast<Args&&>(args)...);
                else
                    static_assert(dependent_false<Self>);
            }

        public:
#define CLU_FIRST_OVERLOAD_CALL_DEF(cnst, ref)                                                                \
            template <typename... Args> requires is_callable_v<cnst first_overload_t ref, Args&&...>          \
            constexpr call_result_t<cnst first_overload_t ref, Args&&...> operator()(Args&&... args) cnst ref \
                noexcept(is_nothrow_callable_v<cnst first_overload_t ref, Args&&...>)                         \
            {                                                                                                 \
                return call_impl<invocable_wrapper<Fs>...>(                                                   \
                    static_cast<cnst first_overload_t ref>(*this),                                            \
                    static_cast<Args&&>(args)...);                                                            \
            } static_assert(true)

            CLU_FIRST_OVERLOAD_CALL_DEF(, &);
            CLU_FIRST_OVERLOAD_CALL_DEF(, &&);
            CLU_FIRST_OVERLOAD_CALL_DEF(const, &);
            CLU_FIRST_OVERLOAD_CALL_DEF(const, &&);
#undef CLU_FIRST_OVERLOAD_CALL_DEF
        };
    }

    template <typename... Fs>
    constexpr auto overload(Fs&&... fns)
    {
        return detail::overload_t<std::decay_t<Fs>...>(
            invocable_wrapper<std::decay_t<Fs>>(static_cast<Fs&&>(fns))...);
    }

    template <typename... Fs>
    constexpr auto first_overload(Fs&&... fns)
    {
        return detail::first_overload_t<std::decay_t<Fs>...>(
            invocable_wrapper<std::decay_t<Fs>>(static_cast<Fs&&>(fns))...);
    }
}
