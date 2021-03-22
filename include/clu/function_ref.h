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
        using ptr_fptr_t = R(*)(void*, Ts ...);
        using fptr_t = R(*)(Ts ...);

        void* ptr_ = nullptr;
        ptr_fptr_t fptr_ = nullptr;

        template <typename F>
        static constexpr R call_impl(void* ptr, Ts ... args)
        {
            return std::invoke(*static_cast<std::remove_cvref_t<F>*>(ptr),
                std::forward<Ts>(args)...);
        }

        template <typename F>
        void assign(F&& func) noexcept
        {
            using stem = std::remove_pointer_t<std::remove_cvref_t<F>>;
            if constexpr (std::is_function_v<stem>)
            {
                ptr_ = nullptr;
                fptr_ = reinterpret_cast<ptr_fptr_t>(static_cast<stem*>(func));
            }
            else
            {
                ptr_ = const_cast<void*>(static_cast<const void*>(std::addressof(func)));
                fptr_ = &function_ref::template call_impl<F>;
            }
        }

    public:
        constexpr function_ref() noexcept = default;
        constexpr explicit(false) function_ref(std::nullptr_t) noexcept {}

        template <typename F> requires (invocable_of<F, R, Ts...> && !similar_to<F, function_ref>)
        explicit(false) function_ref(F&& func) noexcept { assign(std::forward<F>(func)); }

        template <typename F> requires (invocable_of<F, R, Ts...> && !similar_to<F, function_ref>)
        function_ref& operator=(F&& func) noexcept
        {
            assign(std::forward<F>(func));
            return *this;
        }

        constexpr explicit operator bool() const noexcept { return fptr_ != nullptr; }

        R operator()(Ts ... args) const
        {
            if (ptr_)
                return fptr_(ptr_, std::forward<Ts>(args)...);
            else
                return reinterpret_cast<fptr_t>(fptr_)(std::forward<Ts>(args)...);
        }

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
