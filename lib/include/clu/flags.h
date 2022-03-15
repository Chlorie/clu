#pragma once

#include "concepts.h"
#include "type_traits.h"

namespace clu
{
    template <enumeration Enum>
    [[nodiscard]] constexpr auto to_underlying(const Enum value) noexcept
    {
        return static_cast<std::underlying_type_t<Enum>>(value);
    }

    template <typename Enum>
    concept flag_enum = enumeration<Enum> &&(to_underlying(Enum::flags_bit_size) > 0);

    template <typename Enum>
    class flags
    {
        static_assert(flag_enum<Enum>, "type E should be a flag_enum");

    private:
        static constexpr size_t bit_size = static_cast<size_t>(Enum::flags_bit_size);

    public:
        using data_type = decltype(
            []
            {
                if constexpr (bit_size <= 8)
                    return u8{};
                else if constexpr (bit_size <= 16)
                    return u16{};
                else if constexpr (bit_size <= 32)
                    return u32{};
                else if constexpr (bit_size <= 64)
                    return u64{};
                else
                    static_assert(dependent_false<Enum>, "flags with more than 64 bits are not supported");
            }());

    private:
        // Start from 2 to avoid UB when all of the bits might be used
        static constexpr data_type all_set_mask = (static_cast<data_type>(2) << (bit_size - 1)) - 1;
        data_type data_{};

        constexpr explicit flags(const data_type data) noexcept: data_(data) {}

    public:
        constexpr flags() noexcept = default;

        constexpr explicit(false) flags(const Enum bit) noexcept: data_(static_cast<data_type>(1) << to_underlying(bit))
        {
        }

        [[nodiscard]] constexpr static flags none_set() noexcept { return flags(); }
        [[nodiscard]] constexpr static flags all_set() noexcept { return flags(all_set_mask); }

        [[nodiscard]] constexpr static std::size_t size() noexcept { return bit_size; }

        constexpr flags& operator&=(const flags other) noexcept
        {
            data_ &= other.data_;
            return *this;
        }

        constexpr flags& operator|=(const flags other) noexcept
        {
            data_ |= other.data_;
            return *this;
        }

        constexpr flags& operator^=(const flags other) noexcept
        {
            data_ ^= other.data_;
            return *this;
        }

        [[nodiscard]] constexpr friend flags operator&(const flags lhs, const flags rhs) noexcept
        {
            return flags(lhs.data_ & rhs.data_);
        }
        [[nodiscard]] constexpr friend flags operator|(const flags lhs, const flags rhs) noexcept
        {
            return flags(lhs.data_ | rhs.data_);
        }
        [[nodiscard]] constexpr friend flags operator^(const flags lhs, const flags rhs) noexcept
        {
            return flags(lhs.data_ ^ rhs.data_);
        }
        [[nodiscard]] constexpr flags operator~() const noexcept { return flags(all_set_mask ^ data_); }

        [[nodiscard]] constexpr friend bool operator==(flags, flags) noexcept = default;

        constexpr flags& set(const Enum bit) noexcept { return *this |= bit; }
        constexpr flags& reset(const Enum bit) noexcept { return *this &= ~(flags(bit)); }
        constexpr flags& toggle(const Enum bit) noexcept { return *this ^= bit; }
        constexpr flags& toggle_all() noexcept { return *this ^= flags(all_set_mask); }

        [[nodiscard]] constexpr bool is_set(const flags bits) const noexcept
        {
            return (data_ & bits.data_) == bits.data_;
        }
        [[nodiscard]] constexpr bool is_unset(const flags bits) const noexcept
        {
            return (~data_ & bits.data_) == bits.data_;
        }

        [[nodiscard]] constexpr explicit operator data_type() const noexcept { return data_; }
    };

    inline namespace flag_enum_operators
    {
        template <flag_enum Enum>
        [[nodiscard]] constexpr flags<Enum> operator&(const Enum lhs, const Enum rhs) noexcept
        {
            return flags<Enum>(lhs) & rhs;
        }
        template <flag_enum Enum>
        [[nodiscard]] constexpr flags<Enum> operator|(const Enum lhs, const Enum rhs) noexcept
        {
            return flags<Enum>(lhs) | rhs;
        }
        template <flag_enum Enum>
        [[nodiscard]] constexpr flags<Enum> operator^(const Enum lhs, const Enum rhs) noexcept
        {
            return flags<Enum>(lhs) ^ rhs;
        }
        template <flag_enum Enum>
        [[nodiscard]] constexpr flags<Enum> operator~(const Enum bit) noexcept
        {
            return ~(flags<Enum>(bit));
        }
    } // namespace flag_enum_operators
} // namespace clu
