#pragma once

#include <span>
#include <bit>
#include <cstring>

#include "assertion.h"
#include "concepts.h"

namespace clu
{
    /**
     * \brief Specifies that a type is safe to alias.
     * \details An alias-safe type could be used as buffer elements.
     */
    template <typename T>
    concept alias_safe = same_as_any_of<T, unsigned char, char, std::byte>;

    // clang-format off
    /**
     * \brief Specifies that a type is safe to be seen through by a buffer.
     * \details Buffer-safe types can be read from/written into buffers.
     */
    template <typename T>
    concept buffer_safe = 
        trivially_copyable<T> ||
        (std::is_array_v<T> && trivially_copyable<std::remove_all_extents_t<T>>);

    template <typename T>
    concept trivial_range =
        std::ranges::contiguous_range<T> &&
        std::ranges::sized_range<T> &&
        trivially_copyable<std::ranges::range_value_t<T>>;

    template <typename T>
    concept mutable_trivial_range =
        trivial_range<T> &&
        (!std::is_const_v<std::ranges::range_value_t<T>>);
    // clang-format on

    /**
     * \brief Copies data between addresses, safe even when the destination overlaps the source.
     * \param dst Destination address.
     * \param src Source address.
     * \param size The size (counting T's, not bytes) to copy.
     */
    template <alias_safe T>
    constexpr void memmove(T* dst, const T* src, const size_t size) noexcept
    {
        if (std::is_constant_evaluated())
        {
            for (std::size_t i = 0; i < size; ++i)
                if (src + i == dst) // should copy backwards
                {
                    std::copy_backward(src, src + size, dst + size);
                    return;
                }
            std::copy(src, src + size, dst);
        }
        else
            std::memmove(dst, src, size);
    }

    /**
     * \brief Non-owning buffer view, suitable for raw byte manipulations.
     * \tparam T The buffer element type, must be alias-safe.
     */
    template <typename T>
    class basic_buffer final
    {
        static_assert(alias_safe<std::remove_const_t<T>>);

    public:
        using value_type = T;
        using mutable_type = basic_buffer<std::remove_const_t<T>>;
        using const_type = basic_buffer<std::add_const_t<T>>;

        constexpr basic_buffer() noexcept = default; ///< Creates an empty buffer.
        constexpr basic_buffer(const basic_buffer&) noexcept = default;
        constexpr basic_buffer(basic_buffer&&) noexcept = default;
        constexpr basic_buffer& operator=(const basic_buffer&) noexcept = default;
        constexpr basic_buffer& operator=(basic_buffer&&) noexcept = default;

        /**
         * \brief Constructs a buffer from a start pointer and a size.
         * \param ptr Start of the buffer region.
         * \param size Size of the buffer.
         */
        constexpr basic_buffer(T* ptr, const size_t size) noexcept: ptr_(ptr), size_(size) {}

        // clang-format off
        template <typename R> requires
            (std::is_const_v<T> && trivial_range<R>) ||
            (!std::is_const_v<T> && mutable_trivial_range<R>)
        constexpr explicit(false) basic_buffer(R&& range) noexcept:
            ptr_(conditional_reinterpret_cast(std::ranges::data(range))),
            size_(std::ranges::size(range) * sizeof(std::ranges::range_value_t<R>)) {}
        // clang-format on

        template <typename = int>
            requires std::is_const_v<T>
        constexpr explicit(false) basic_buffer(const mutable_type& buffer) noexcept:
            ptr_(buffer.ptr()), size_(buffer.size())
        {
        }

        [[nodiscard]] constexpr T* data() const noexcept { return ptr_; } ///< Gets a pointer to the buffer data.
        [[nodiscard]] constexpr size_t size() const noexcept { return size_; } ///< Gets the size of the buffer.
        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; } ///< Checks if the buffer is empty.
        constexpr void remove_prefix(const size_t size) noexcept { ptr_ += size; }
        constexpr void remove_suffix(const size_t size) noexcept { size_ -= size; }
        [[nodiscard]] constexpr T& operator[](const size_t index) const noexcept { return ptr_[index]; }
        [[nodiscard]] constexpr basic_buffer first(const size_t size) const noexcept { return {ptr_, size}; }
        [[nodiscard]] constexpr basic_buffer last(const size_t size) const noexcept
        {
            return {ptr_ + (size_ - size), size};
        }

        constexpr basic_buffer& operator+=(const size_t size) noexcept
        {
            CLU_ASSERT(size_ >= size, "size to consume is too large");
            ptr_ += size;
            return *this;
        }
        [[nodiscard]] constexpr friend basic_buffer operator+(basic_buffer buffer, const size_t size) noexcept
        {
            return buffer += size;
        }

        /**
         * \brief Converts the buffer into a `std::span` of the element type.
         */
        [[nodiscard]] constexpr std::span<T> as_span() const noexcept { return {ptr_, size_}; }

        template <trivially_copyable U>
        [[nodiscard]] constexpr U as() const noexcept
        {
            CLU_ASSERT(size() == sizeof(U), "mismatched size");
            if (std::is_constant_evaluated())
            {
                std::remove_const_t<T> arr[sizeof(U)]{};
                for (size_t i = 0; i < sizeof(U); i++)
                    arr[i] = ptr_[i];
                return std::bit_cast<U>(arr);
            }
            else
                return std::bit_cast<U>(*reinterpret_cast<const T(*)[sizeof(U)]>(ptr_));
        }

        template <trivially_copyable U>
        [[nodiscard]] constexpr U consume_as() noexcept
        {
            CLU_ASSERT(size() >= sizeof(U), "buffer length not enough");
            auto result = first(sizeof(U)).template as<U>();
            ptr_ += sizeof(U);
            return result;
        }

        constexpr size_t copy_to(const mutable_type dest) const noexcept
        {
            const size_t copy_size = std::min(size_, dest.size());
            clu::memmove(dest.data(), data(), copy_size);
            return copy_size;
        }

        constexpr size_t copy_to_consume(const mutable_type dest) noexcept
        {
            const size_t copy_size = copy_to(dest);
            ptr_ += copy_size;
            return copy_size;
        }

    private:
        T* ptr_ = nullptr;
        size_t size_ = 0;

        template <typename U>
        constexpr static T* conditional_reinterpret_cast(U* ptr) noexcept
        {
            if constexpr (clu::similar_to<T, U>)
                return ptr;
            else
                return reinterpret_cast<T*>(ptr);
        }
    };

    using mutable_buffer = basic_buffer<std::byte>; ///< Mutable buffer using `std::byte` as the element type.
    using const_buffer = basic_buffer<const std::byte>; ///< Constant buffer using `std::byte` as the element type.

    template <buffer_safe T>
    [[nodiscard]] mutable_buffer trivial_buffer(T& value) noexcept
    {
        return {reinterpret_cast<std::byte*>(std::addressof(value)), sizeof(T)};
    }

    template <buffer_safe T>
    [[nodiscard]] const_buffer trivial_buffer(const T& value) noexcept
    {
        return {reinterpret_cast<const std::byte*>(std::addressof(value)), sizeof(T)};
    }

    template <typename T>
    void trivial_buffer(const T&&) = delete;
} // namespace clu
