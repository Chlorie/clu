#pragma once

#include <new>

namespace clu
{
    template <typename T>
    [[nodiscard]] static void* aligned_alloc_for()
    {
        if constexpr (alignof(T) > alignof(std::max_align_t))
            return ::operator new(sizeof(T), std::align_val_t{alignof(T)});
        else
            return ::operator new(sizeof(T));
    }

    template <typename T>
    static void aligned_free_for(void* ptr)
    {
        if constexpr (alignof(T) > alignof(std::max_align_t))
            ::operator delete(ptr, std::align_val_t{alignof(T)});
        else
            ::operator delete(ptr);
    }

    template <typename T>
    [[nodiscard]] static void* aligned_alloc_for(const std::size_t count)
    {
        if constexpr (alignof(T) > alignof(std::max_align_t))
            return ::operator new(sizeof(T) * count, std::align_val_t{alignof(T)});
        else
            return ::operator new(sizeof(T) * count);
    }

    template <typename T>
    static void aligned_free_for(void* ptr, const std::size_t count)
    {
        if constexpr (alignof(T) > alignof(std::max_align_t))
            ::operator delete(ptr, sizeof(T) * count, std::align_val_t{alignof(T)});
        else
            ::operator delete(ptr, sizeof(T) * count);
    }

    constexpr std::size_t align_offset(const std::size_t offset, const std::size_t alignment) noexcept
    {
        return (offset + alignment - 1) / alignment * alignment;
    }
} // namespace clu
