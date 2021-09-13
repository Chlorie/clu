#pragma once

#include <cstddef>
#include <string_view>

namespace clu
{
    template <typename T, std::size_t N>
    struct basic_fixed_string
    {
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<T*>;
        using const_reverse_iterator = std::reverse_iterator<const T*>;

        using view = std::basic_string_view<T>;

        T data_[N + 1];

        constexpr basic_fixed_string() noexcept: data_{} {}

        constexpr explicit(false) basic_fixed_string(const T (& str)[N + 1]) noexcept
        {
            for (std::size_t i = 0; i <= N; i++)
                data_[i] = str[i];
        }

        constexpr basic_fixed_string& operator=(const T (& str)[N + 1]) noexcept
        {
            for (std::size_t i = 0; i <= N; i++)
                data_[i] = str[i];
            return *this;
        }

        [[nodiscard]] constexpr T* begin() noexcept { return data_; }
        [[nodiscard]] constexpr T* end() noexcept { return data_ + N; }
        [[nodiscard]] constexpr const T* begin() const noexcept { return data_; }
        [[nodiscard]] constexpr const T* end() const noexcept { return data_ + N; }
        [[nodiscard]] constexpr const T* cbegin() const noexcept { return data_; }
        [[nodiscard]] constexpr const T* cend() const noexcept { return data_ + N; }
        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr reverse_iterator rend() noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return std::make_reverse_iterator(end()); }
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return std::make_reverse_iterator(begin()); }
        [[nodiscard]] constexpr T& front() noexcept { return data_[0]; }
        [[nodiscard]] constexpr const T& front() const noexcept { return data_[0]; }
        [[nodiscard]] constexpr T& back() noexcept { return data_[N]; }
        [[nodiscard]] constexpr const T& back() const noexcept { return data_[N]; }
        [[nodiscard]] constexpr const T* c_str() const noexcept { return data_; }
        [[nodiscard]] constexpr T* data() noexcept { return data_; }
        [[nodiscard]] constexpr const T* data() const noexcept { return data_; }
        [[nodiscard]] constexpr T& operator[](const std::size_t index) noexcept { return data_[index]; }
        [[nodiscard]] constexpr const T& operator[](const std::size_t index) const noexcept { return data_[index]; }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return N; }
        [[nodiscard]] constexpr std::size_t length() const noexcept { return N; }
        [[nodiscard]] constexpr bool operator==(const basic_fixed_string&) const noexcept = default;
        [[nodiscard]] constexpr auto operator<=>(const basic_fixed_string&) const noexcept = default;
        [[nodiscard]] constexpr explicit(false) operator view() const noexcept { return view(data_, N); }

        template <std::size_t M>
        [[nodiscard]] constexpr friend basic_fixed_string<T, N + M> operator+(
            const basic_fixed_string<T, N>& lhs, const basic_fixed_string<T, M>& rhs) noexcept
        {
            basic_fixed_string<T, N + M> result{};
            for (std::size_t i = 0; i < N; i++) result[i] = lhs[i];
            for (std::size_t i = 0; i <= M; i++) result[i + N] = rhs[i];
            return result;
        }

        template <std::size_t M>
        [[nodiscard]] constexpr friend basic_fixed_string<T, N + M - 1> operator+(
            const basic_fixed_string<T, N>& lhs, const T (& rhs)[M]) noexcept
        {
            basic_fixed_string<T, N + M - 1> result{};
            for (std::size_t i = 0; i < N; i++) result[i] = lhs[i];
            for (std::size_t i = 0; i < M; i++) result[i + N] = rhs[i];
            return result;
        }

        template <std::size_t M>
        [[nodiscard]] constexpr friend basic_fixed_string<T, N + M - 1> operator+(
            const T (& lhs)[M], const basic_fixed_string<T, N>& rhs) noexcept
        {
            basic_fixed_string<T, N + M - 1> result{};
            for (std::size_t i = 0; i < M - 1; i++) result[i] = lhs[i];
            for (std::size_t i = 0; i <= N; i++) result[i + M - 1] = rhs[i];
            return result;
        }
    };

    template <typename T, std::size_t N>
    basic_fixed_string(const T (&)[N]) -> basic_fixed_string<T, N - 1>;

    template <std::size_t N> using fixed_string = basic_fixed_string<char, N>;
    template <std::size_t N> using fixed_wstring = basic_fixed_string<wchar_t, N>;
    template <std::size_t N> using fixed_u8string = basic_fixed_string<char8_t, N>;
    template <std::size_t N> using fixed_u16string = basic_fixed_string<char16_t, N>;
    template <std::size_t N> using fixed_u32string = basic_fixed_string<char32_t, N>;

    inline namespace literals
    {
        inline namespace fixed_string_literals
        {
            template <basic_fixed_string str>
            [[nodiscard]] constexpr auto operator""_fs() { return str; }
        }
    }
}
