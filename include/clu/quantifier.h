#pragma once

#include <cstdint>
#include <utility>
#include <tuple>

namespace clu
{
    namespace detail
    {
        enum class quantifier_type : uint8_t { all, any, none };

        template <quantifier_type Q, typename... Ts>
        class elements final
        {
        private:
            std::tuple<Ts...> values_;

            template <typename U, typename F, size_t... Idx>
            constexpr bool check_impl(const U& value, const F& func, std::index_sequence<Idx...>) const
            {
                if constexpr (Q == quantifier_type::all)
                    return (func(std::get<Idx>(values_), value) && ...);
                else if constexpr (Q == quantifier_type::any)
                    return (func(std::get<Idx>(values_), value) || ...);
                else
                    return !(func(std::get<Idx>(values_), value) || ...);
            }

            template <typename U, typename F>
            constexpr bool check(const U& value, const F& func) const
            {
                static constexpr auto seq = std::make_index_sequence<sizeof...(Ts)>{};
                return check_impl(value, func, seq);
            }

        public:
            explicit constexpr elements(Ts&&... values): values_{ std::forward<Ts>(values)... } {}

            template <typename U>
            friend constexpr bool operator==(const elements& lhs, const U& rhs)
            {
                return lhs.check(rhs, std::equal_to<>{});
            }
            template <typename U>
            friend constexpr bool operator==(const U& lhs, const elements& rhs)
            {
                return rhs == lhs;
            }
            template <typename U>
            friend constexpr bool operator!=(const elements& lhs, const U& rhs)
            {
                return lhs.check(rhs, std::not_equal_to<>{});
            }
            template <typename U>
            friend constexpr bool operator!=(const U& lhs, const elements& rhs)
            {
                return rhs != lhs;
            }
            template <typename U>
            friend constexpr bool operator<(const elements& lhs, const U& rhs)
            {
                return lhs.check(rhs, std::less<>{});
            }
            template <typename U>
            friend constexpr bool operator>(const U& lhs, const elements& rhs)
            {
                return rhs < lhs;
            }
            template <typename U>
            friend constexpr bool operator>(const elements& lhs, const U& rhs)
            {
                return lhs.check(rhs, std::greater<>{});
            }
            template <typename U>
            friend constexpr bool operator<(const U& lhs, const elements& rhs)
            {
                return rhs > lhs;
            }
            template <typename U>
            friend constexpr bool operator<=(const elements& lhs, const U& rhs)
            {
                return lhs.check(rhs, std::less_equal<>{});
            }
            template <typename U>
            friend constexpr bool operator>=(const U& lhs, const elements& rhs)
            {
                return rhs <= lhs;
            }
            template <typename U>
            friend constexpr bool operator>=(const elements& lhs, const U& rhs)
            {
                return lhs.check(rhs, std::greater_equal<>{});
            }
            template <typename U>
            friend constexpr bool operator<=(const U& lhs, const elements& rhs)
            {
                return rhs >= lhs;
            }
        };
    }

    template <typename... Ts>
    constexpr auto all_of(Ts&&... values)
    {
        return detail::elements<detail::quantifier_type::all, Ts&&...>{ std::forward<Ts>(values)... };
    }
    template <typename... Ts>
    constexpr auto any_of(Ts&&... values)
    {
        return detail::elements<detail::quantifier_type::any, Ts&&...>{ std::forward<Ts>(values)... };
    }
    template <typename... Ts>
    constexpr auto none_of(Ts&&... values)
    {
        return detail::elements<detail::quantifier_type::none, Ts&&...>{ std::forward<Ts>(values)... };
    }
}
