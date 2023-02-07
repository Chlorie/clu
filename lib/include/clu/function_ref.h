#pragma once

#include <functional>

#include "macros.h"
#include "concepts.h"

namespace clu
{
    template <typename Func>
    class function_ref;

    /// Non-owning type erased wrapper for invocable objects.
    template <typename R, typename... Ts>
    class function_ref<R(Ts...)> final
    {
    private:
        using ptr_fptr_t = R (*)(void*, Ts...);
        using fptr_t = R (*)(Ts...);

        void* ptr_ = nullptr;
        ptr_fptr_t fptr_ = nullptr;

        template <typename F>
        static constexpr R call_impl(void* ptr, Ts... args)
        {
            return std::invoke(*static_cast<std::remove_cvref_t<F>*>(ptr), std::forward<Ts>(args)...);
        }

        template <typename F>
        void assign(F&& func) noexcept
        {
            using stem = std::remove_pointer_t<std::remove_cvref_t<F>>;
            if constexpr (std::is_function_v<stem>)
            {
                ptr_ = nullptr;
                CLU_GCC_WNO_CAST_FUNCTION_TYPE
                fptr_ = reinterpret_cast<ptr_fptr_t>(static_cast<stem*>(func));
                CLU_GCC_RESTORE_WARNING
            }
            else
            {
                ptr_ = const_cast<void*>(static_cast<const void*>(std::addressof(func)));
                fptr_ = &function_ref::template call_impl<F>;
            }
        }

    public:
        /// \group empty_ctors
        /// Construct an empty `function_ref`. Calling thus an object leads to undefined behavior.
        constexpr function_ref() noexcept = default;
        constexpr explicit(false) function_ref(std::nullptr_t) noexcept {} //< \group empty_ctors

        template <typename F>
            requires(invocable_of<F, R, Ts...> && !similar_to<F, function_ref>)
        explicit(false) function_ref(F&& func) noexcept { this->assign(std::forward<F>(func)); }

        template <typename F>
            requires(invocable_of<F, R, Ts...> && !similar_to<F, function_ref>)
        function_ref& operator=(F&& func) noexcept
        {
            this->assign(std::forward<F>(func));
            return *this;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return fptr_ != nullptr; }

        R operator()(Ts... args) const
        {
            if (ptr_)
                return fptr_(ptr_, std::forward<Ts>(args)...);
            else
            {
                CLU_GCC_WNO_CAST_FUNCTION_TYPE
                return reinterpret_cast<fptr_t>(fptr_)(std::forward<Ts>(args)...);
                CLU_GCC_RESTORE_WARNING
            }
        }

        constexpr void swap(function_ref& other) noexcept
        {
            std::swap(ptr_, other.ptr_);
            std::swap(fptr_, other.fptr_);
        }

        friend constexpr void swap(function_ref& lhs, function_ref& rhs) noexcept { lhs.swap(rhs); }
    };

    template <typename R, typename... Ts>
    function_ref(R (*)(Ts...)) -> function_ref<R(Ts...)>;
} // namespace clu
