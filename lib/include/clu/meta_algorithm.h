#pragma once

#include <compare>

#include "type_traits.h"
#include "meta_list.h"

namespace clu::meta
{
    struct npos_t
    {
        friend constexpr bool operator==(npos_t, npos_t) noexcept = default;
        friend constexpr auto operator<=>(npos_t, npos_t) noexcept = default;
        constexpr bool operator==(size_t) const noexcept { return false; }
    };
    inline constexpr npos_t npos{};

    template <size_t idx, typename Type>
    struct indexed_type
    {
        static constexpr size_t index = idx;
        using type = Type;
    };

    namespace detail
    {
        template <typename T> struct add_one_t : value_tag_t<npos> {};
        template <typename T> requires std::integral<typename T::value_type>
        struct add_one_t<T> : std::integral_constant<typename T::value_type, T::value + 1> {};
    }

    template <typename List> struct extract_types {};
    template <template <typename...> typename Templ, typename... Types>
    struct extract_types<Templ<Types...>> : std::type_identity<type_list<Types...>> {};
    template <typename List> using extract_types_t = typename extract_types<List>::type;

    template <typename Invocable, typename List> using invoke = typename List::template apply<Invocable::template apply>;
    template <typename Invocable, typename List> using invoke_t = typename List::template apply<Invocable::template apply>::type;
    template <typename Invocable, typename List> static constexpr auto invoke_v = List::template apply<Invocable::template apply>::value;

    namespace detail
    {
        template <typename... Types>
        struct enumerate
        {
            using type = decltype([]<size_t... idx>(std::index_sequence<idx...>)
            {
                return type_list<indexed_type<idx, Types>...>{};
            }(std::make_index_sequence<sizeof...(Types)>{}));
        };

        template <typename Target>
        struct contains
        {
            template <typename... Types> static constexpr bool apply_v = (std::is_same_v<Target, Types> || ...);
            template <typename... Types> using apply = std::bool_constant<apply_v<Types...>>;
        };

        template <template <typename> typename Pred>
        struct all_of
        {
            template <typename... Types> static constexpr bool apply_v = (Pred<Types>::value && ...);
            template <typename... Types> using apply = std::integral_constant<size_t, apply_v<Types...>>;
        };

        template <template <typename> typename Pred>
        struct any_of
        {
            template <typename... Types> static constexpr bool apply_v = (Pred<Types>::value || ...);
            template <typename... Types> using apply = std::integral_constant<size_t, apply_v<Types...>>;
        };

        template <template <typename> typename Pred>
        struct none_of
        {
            template <typename... Types> static constexpr bool apply_v = !(Pred<Types>::value || ...);
            template <typename... Types> using apply = std::integral_constant<size_t, apply_v<Types...>>;
        };

        template <typename Target>
        struct find
        {
            template <typename... Types> struct apply : value_tag_t<npos> {};
            template <typename... Types> static constexpr auto apply_v = apply<Types...>::value;
        };
        template <typename Target> template <typename... Rest>
        struct find<Target>::apply<Target, Rest...> : std::integral_constant<size_t, 0> {};
        template <typename Target> template <typename First, typename... Rest>
        struct find<Target>::apply<First, Rest...> : add_one_t<apply<Rest...>> {};

        template <typename Target>
        struct count
        {
            template <typename... Types> static constexpr auto apply_v = (std::is_same_v<Target, Types> + ... + 0);
            template <typename... Types> using apply = std::integral_constant<size_t, apply_v<Types...>>;
        };

        template <template <typename> typename Pred>
        struct count_if
        {
            template <typename... Types> static constexpr auto apply_v = (static_cast<size_t>(Pred<Types>::value) + ... + 0);
            template <typename... Types> using apply = std::integral_constant<size_t, apply_v<Types...>>;
        };

        template <template <typename> typename Pred>
        struct filter
        {
            template <typename... Types> struct apply : std::type_identity<type_list<>> {};
            template <typename... Types> using apply_t = typename apply<Types...>::type;
        };
        template <template <typename> typename Pred>
        template <typename First, typename... Rest>
        struct filter<Pred>::apply<First, Rest...>
        {
            using type = conditional_t<
                Pred<First>::value,
                typename apply<Rest...>::type::template concatenate_front<First>,
                typename apply<Rest...>::type>;
        };

        template <typename... Types> struct is_unique : std::true_type {};
        template <typename First, typename... Rest> struct is_unique<First, Rest...> :
            std::bool_constant<!(std::is_same_v<First, Rest> || ...) && is_unique<Rest...>::value> {};

        template <typename... Types> struct unique : std::type_identity<type_list<>> {};
        template <typename First, typename... Rest>
        struct unique<First, Rest...>
        {
            using type = conditional_t<
                (std::is_same_v<First, Rest> || ...),
                typename unique<Rest...>::type,
                typename unique<Rest...>::type::template concatenate_front<First>>;
        };
    }

    template <typename List> using enumerate_t = typename List::template apply_t<detail::enumerate>;
    template <typename List> using enumerate = std::type_identity<enumerate_t<List>>;

    template <typename List, typename T> inline constexpr bool contains_v = List::template apply_v<detail::contains<T>::template apply>;
    template <typename List, typename T> using contains = std::bool_constant<contains_v<List, T>>;

    template <typename List, template <typename> typename Pred>
    inline constexpr bool all_of_v = List::template apply_v<detail::all_of<Pred>::template apply>;
    template <typename List, template <typename> typename Pred> using all_of = std::bool_constant<all_of_v<List, Pred>>;

    template <typename List, template <typename> typename Pred>
    inline constexpr bool any_of_v = List::template apply_v<detail::any_of<Pred>::template apply>;
    template <typename List, template <typename> typename Pred> using any_of = std::bool_constant<any_of_v<List, Pred>>;

    template <typename List, template <typename> typename Pred>
    inline constexpr bool none_of_v = List::template apply_v<detail::none_of<Pred>::template apply>;
    template <typename List, template <typename> typename Pred> using none_of = std::bool_constant<none_of_v<List, Pred>>;

    template <typename List, typename T> inline constexpr auto find_v = List::template apply_v<detail::find<T>::template apply>;
    template <typename List, typename T> using find = value_tag_t<find_v<List, T>>;

    template <typename List, typename T> inline constexpr size_t count_v = List::template apply<detail::count<T>::template apply>;
    template <typename List, typename T> using count = std::integral_constant<size_t, count_v<List, T>>;

    template <typename List1, typename List2>
    using concatenate_t = typename List1::template apply<List2::template concatenate_front>;
    template <typename List1, typename List2> using concatenate = std::type_identity<concatenate_t<List1, List2>>;

    template <typename List, template <typename> typename Pred>
    inline constexpr size_t count_if_v = List::template apply_v<detail::count_if<Pred>::template apply>;
    template <typename List, template <typename> typename Pred> using count_if = std::integral_constant<size_t, count_if_v<List, Pred>>;

    template <typename List, template <typename> typename Pred>
    using filter_t = typename List::template apply_t<detail::filter<Pred>::template apply>;
    template <typename List, template <typename> typename Pred> using filter = std::type_identity<filter_t<List, Pred>>;

    template <typename List> inline constexpr bool is_unique_v = List::template apply_v<detail::is_unique>;
    template <typename List> using is_unique = std::bool_constant<is_unique_v<List>>;

    template <typename List> using unique_t = typename List::template apply_t<detail::unique>;
    template <typename List> using unique = std::type_identity<unique_t<List>>;

    // Check whether Set1 includes Set2
    // Preconditions: Set1 and Set2 are type sets
    template <typename Set1, typename Set2> struct set_include;
    template <typename Set1, typename... Types2>
    struct set_include<Set1, type_list<Types2...>> : std::bool_constant<(Set1::template contains_v<Types2> && ...)> {};
    template <typename Set1, typename Set2> inline constexpr bool set_include_v = set_include<Set1, Set2>::value;

    template <typename Set1, typename Set2> inline constexpr bool set_equal_v = Set1::size == Set2::size && set_include_v<Set1, Set2>;
    template <typename Set1, typename Set2> using set_equal = std::integral_constant<size_t, set_equal_v<Set1, Set2>>;

    template <typename Set1, typename Set2> using set_union_t = unique_t<concatenate_t<Set1, Set2>>;
    template <typename Set1, typename Set2> using set_union = std::type_identity<set_union_t<Set1, Set2>>;

    template <typename Set1, typename Set2> struct set_intersection;
    template <typename Set1> struct set_intersection<Set1, type_list<>> : std::type_identity<type_list<>> {};
    template <typename Set1, typename First, typename... Rest>
    struct set_intersection<Set1, type_list<First, Rest...>>
    {
        using type = conditional_t<
            Set1::template contains_v<First>,
            typename set_intersection<Set1, type_list<Rest...>>::type::template concatenate_front<First>,
            typename set_intersection<Set1, type_list<Rest...>>::type>;
    };
    template <typename Set1, typename Set2> using set_intersection_t = typename set_intersection<Set1, Set2>::type;

    template <typename Set1, typename Set2> struct set_difference;
    template <typename Set2> struct set_difference<type_list<>, Set2> : std::type_identity<type_list<>> {};
    template <typename First, typename... Rest, typename Set2>
    struct set_difference<type_list<First, Rest...>, Set2>
    {
        using type = conditional_t<
            Set2::template contains_v<First>,
            typename set_difference<type_list<Rest...>, Set2>::type,
            typename set_difference<type_list<Rest...>, Set2>::type::template concatenate_front<First>>;
    };
    template <typename Set1, typename Set2> using set_difference_t = typename set_difference<Set1, Set2>::type;

    template <typename Set1, typename Set2>
    using set_symmetric_difference_t = concatenate_t<set_difference_t<Set1, Set2>, set_difference_t<Set2, Set1>>;
    template <typename Set1, typename Set2> using set_symmetric_difference = std::type_identity<set_symmetric_difference_t<Set1, Set2>>;
}
