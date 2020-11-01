#pragma once

#include <functional>

#include "concepts.h"

namespace clu
{
    template <typename Func>
    class function_ref;

    template <typename R, typename... Ts>
    class function_ref<R(Ts ...)> final
    {
    private:
        void* ptr_ = nullptr;
        R (*fptr_)(void*, Ts ...) = nullptr;

        template <typename F>
        static constexpr R call_impl(void* ptr, Ts ... args)
        {
            return std::invoke(*static_cast<std::remove_cvref_t<F>*>(ptr), std::forward<Ts>(args)...);
        }

    public:
        function_ref() = delete;

        template <typename F> requires invocable_of<F, R, Ts...> && !similar_to<F, function_ref>
        explicit(false) constexpr function_ref(F&& func) noexcept: // NOLINT(bugprone-forwarding-reference-overload)
            ptr_(const_cast<void*>(static_cast<const void*>(std::addressof(func)))), // TODO: no non-standard conversions
            fptr_(&function_ref::template call_impl<F>) {}

        template <typename F> requires invocable_of<F, R, Ts...> && !similar_to<F, function_ref>
        constexpr function_ref& operator=(F&& func) noexcept
        {
            ptr_ = const_cast<void*>(static_cast<const void*>(std::addressof(func)));
            fptr_ = &function_ref::template call_impl<F>;
            return *this;
        }

        constexpr R operator()(Ts ... args) const { return fptr_(ptr_, std::forward<Ts>(args)...); }

        constexpr void swap(function_ref& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            std::swap(fptr_, other.fptr_);
        }

        friend constexpr void swap(function_ref& lhs, function_ref& rhs) noexcept { lhs.swap(rhs); }
    };

    template <typename R, typename... Ts>
    function_ref(R (*)(Ts ...)) -> function_ref<R(Ts ...)>;
}
