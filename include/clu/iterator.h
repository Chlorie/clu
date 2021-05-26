#pragma once

#include <iterator>

#include "type_traits.h"
#include "concepts.h"

namespace clu
{
    namespace detail
    {
        // TODO: requires expression when MSVC updates
        template <typename It> concept has_iterator_category = requires { typename It::iterator_category; };
        template <typename It> concept has_iterator_concept = requires { typename It::iterator_concept; };
        template <typename It> concept has_value_type = requires { typename It::value_type; };
        template <typename It> concept has_difference_type = requires { typename It::difference_type; };
        template <typename It> concept distance_computable = requires(It a, It b) { b - a; };
        template <typename It> concept has_increment = requires(It a) { ++a; };
        template <typename It> concept has_decrement = requires(It a) { --a; };
        template <typename It, typename Diff> concept has_subtract_assign = requires(It a, Diff d) { a -= d; };
        template <typename It, typename Diff> concept has_subscript = requires(It a, Diff d) { a[d]; };
    }

    template <typename It>
    class iterator_adapter : public It
    {
    private:
        constexpr static auto get_iterator_category() noexcept
        {
            if constexpr (detail::has_iterator_category<It>)
                return typename It::iterator_category{};
            else if constexpr (detail::has_iterator_concept<It>)
                return typename It::iterator_concept{};
            else if constexpr (!std::equality_comparable<It>) // not forward iterator
                return std::input_iterator_tag{};
            else if constexpr (detail::distance_computable<It>) // random access iterator
                return std::random_access_iterator_tag{};
            else if constexpr (detail::has_decrement<It>) // bidirectional iterator
                return std::bidirectional_iterator_tag{};
            else
                return std::forward_iterator_tag{};
        }

        constexpr static auto get_iterator_concept() noexcept
        {
            if constexpr (detail::has_iterator_concept<It>)
                return typename It::iterator_concept{};
            else
                return get_iterator_category();
        }

        constexpr static auto get_value_type() noexcept
        {
            if constexpr (detail::has_value_type<It>)
                return type_tag<typename It::value_type>;
            else
                return type_tag<std::remove_cvref_t<decltype(*std::declval<It>())>>;
        }

        constexpr static auto get_difference_type() noexcept
        {
            if constexpr (detail::has_difference_type<It>)
                return type_tag<typename It::difference_type>;
            else if constexpr (detail::distance_computable<It>)
                return type_tag<decltype(std::declval<It>() - std::declval<It>())>;
            else
                return type_tag<std::ptrdiff_t>;
        }

        template <typename... Ts>
        constexpr static bool enable_implicit_ctor()
        {
            if constexpr (implicitly_constructible_from<It, Ts...>) return true;
            if constexpr (sizeof...(Ts) == 1) return std::convertible_to<Ts..., It>;
            return false;
        }

    public:
        using base_type = It;

        using iterator_category = decltype(get_iterator_category());
        using iterator_concept = decltype(get_iterator_concept());
        using value_type = typename decltype(get_value_type())::type;
        using difference_type = typename decltype(get_difference_type())::type;

        template <typename... Ts> requires std::constructible_from<It, Ts&&...>
        constexpr explicit(!enable_implicit_ctor<Ts...>()) iterator_adapter(Ts&&... args): It(std::forward<Ts>(args)...) {}

        [[nodiscard]] constexpr auto operator->() const noexcept(noexcept(*std::declval<It&>()))
            requires std::derived_from<iterator_category, std::input_iterator_tag> { return std::addressof(**this); }

        constexpr iterator_adapter& operator++()
        {
            if constexpr (!detail::has_increment<It>)
                It::operator+=(1);
            else
                It::operator++();
            return *this;
        }

        constexpr iterator_adapter operator++(int)
        {
            iterator_adapter result = *this;
            ++*this;
            return result;
        }

        constexpr iterator_adapter& operator--()
            requires std::derived_from<iterator_category, std::bidirectional_iterator_tag>
        {
            if constexpr (!detail::has_increment<It>)
                It::operator+=(-1);
            else
                It::operator--();
            return *this;
        }

        constexpr iterator_adapter operator--(int)
            requires std::derived_from<iterator_category, std::bidirectional_iterator_tag>
        {
            iterator_adapter result = *this;
            --*this;
            return result;
        }

        constexpr iterator_adapter& operator+=(const difference_type value)
            requires std::derived_from<iterator_category, std::random_access_iterator_tag>
        {
            It::operator+=(value);
            return *this;
        }

        constexpr iterator_adapter& operator-=(const difference_type value)
            requires std::derived_from<iterator_category, std::random_access_iterator_tag>
        {
            if constexpr (detail::has_subtract_assign<It, difference_type>)
                It::operator-=(value);
            else
                It::operator+=(-value);
            return *this;
        }

        [[nodiscard]] constexpr friend iterator_adapter operator+(iterator_adapter it, const difference_type value)
            requires std::derived_from<iterator_category, std::random_access_iterator_tag>
        {
            it.It::operator+=(value);
            return it;
        }

        [[nodiscard]] constexpr friend iterator_adapter operator+(const difference_type value, iterator_adapter it)
            requires std::derived_from<iterator_category, std::random_access_iterator_tag>
        {
            it.It::operator+=(value);
            return it;
        }

        [[nodiscard]] constexpr friend iterator_adapter operator-(iterator_adapter it, const difference_type value)
            requires std::derived_from<iterator_category, std::random_access_iterator_tag>
        {
            if constexpr (detail::has_subtract_assign<It, difference_type>)
                it.It::operator-=(value);
            else
                it.It::operator+=(-value);
            return it;
        }

        [[nodiscard]] constexpr decltype(auto) operator[](const difference_type value) const
            requires std::derived_from<iterator_category, std::random_access_iterator_tag>
        {
            if constexpr (detail::has_subscript<It, difference_type>)
                return It::operator[](value);
            else
                return *(*this + value);
        }

        [[nodiscard]] constexpr It& base() noexcept { return *this; }
        [[nodiscard]] constexpr const It& base() const noexcept { return *this; }
    };
}
