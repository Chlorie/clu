#pragma once

#include <functional>

namespace clu
{
    template <typename Func>
    class function_ref;

    template <typename R, typename... Ts>
    class function_ref<R(Ts ...)> final
    {
    private:
        template <typename F>
        static constexpr bool enable_convert =
            std::is_invocable_r_v<R, F, Ts...> && !std::is_same_v<std::decay_t<F>, function_ref>;

        void* ptr_ = nullptr;
        R (*fptr_)(void*, Ts ...) = nullptr;

        template <typename F>
        static R call_impl(void* ptr, Ts ... args)
        {
            return std::invoke(*static_cast<F*>(ptr), std::forward<Ts>(args)...);
        }

    public:
        function_ref() = delete;

        template <typename F, std::enable_if_t<enable_convert>* = nullptr>
        constexpr function_ref(F&& func) noexcept: // NOLINT(bugprone-forwarding-reference-overload)
            ptr_(const_cast<void*>(static_cast<const void*>(std::addressof(func)))),
            fptr_(&function_ref::template call_impl<F>) {}

        template <typename F, std::enable_if_t<enable_convert>* = nullptr>
        constexpr function_ref& operator=(F&& func) noexcept
        {
            ptr_ = const_cast<void*>(static_cast<const void*>(std::addressof(func)));
            fptr_ = &function_ref::template call_impl<F>;
            return *this;
        }

        R operator()(Ts ... args) const { return fptr_(ptr_, std::forward<Ts>(args)...); }

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
