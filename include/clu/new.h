#pragma once

#include <new>

namespace clu
{
    template <typename T>
    [[nodiscard]] static void* aligned_alloc_for()
    {
        if constexpr (alignof(T) > alignof(std::max_align_t))
            return ::operator new(sizeof(T), std::align_val_t{ alignof(T) });
        else
            return ::operator new(sizeof(T));
    }

    template <typename T>
    [[nodiscard]] static void aligned_free_for(void* ptr)
    {
        if constexpr (alignof(T) > alignof(std::max_align_t))
            ::operator delete(ptr, std::align_val_t{ alignof(T) });
        else
            ::operator delete(ptr);
    }
}
