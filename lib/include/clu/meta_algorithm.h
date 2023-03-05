#pragma once

#include <compare>

#include "concepts.h"
#include "meta_list.h"
#include "integer_literals.h"

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
        // clang-format off
        template <typename T> struct add_one_t : value_tag_t<npos> {};
        template <typename T>
            requires std::integral<typename T::value_type>
        struct add_one_t<T> : std::integral_constant<typename T::value_type, T::value + 1> {};
        // clang-format on

        template <bool>
        struct indirection
        {
            template <template <typename...> typename T, typename... As>
            using type = T<As...>;
        };
    } // namespace detail

    template <typename T>
    inline constexpr auto _v = T::value;

    template <template <typename...> typename Fn, typename... Ts>
    using invoke_unquoted = typename detail::indirection<dependent_false<Ts...>>::template type<Fn, Ts...>;
    template <template <typename...> typename Fn, typename... Ts>
    inline constexpr auto invoke_unquoted_v = invoke_unquoted<Fn, Ts...>::value;

    template <template <typename...> typename Fn>
    struct quote
    {
        template <typename... Ts>
        using fn = invoke_unquoted<Fn, Ts...>;
    };

    template <typename Fn>
    using requote = quote<Fn::template fn>;

    template <typename Fn, typename... Ts>
    using invoke = typename Fn::template fn<Ts...>;
    template <typename Fn, typename... Ts>
    inline constexpr auto invoke_v = Fn::template fn<Ts...>::value;

    // clang-format off
    template <typename T> using id = T;
    using id_q = quote<id>;
    template <typename T> using _t = typename T::type;
    using _t_q = quote<_t>;
    // clang-format on

    template <typename Pred, typename TrueTrans, typename FalseTrans = id_q>
    struct if_q
    {
        template <typename... Ts>
        using fn = invoke<conditional_t<invoke<Pred, Ts...>::value, TrueTrans, FalseTrans>, Ts...>;
    };

    template <typename Fn, typename T>
    struct bind_front_q
    {
        template <typename... Us>
        using fn = invoke<Fn, T, Us...>;
    };

    template <typename Fn, typename T>
    struct bind_back_q
    {
        template <typename... Us>
        using fn = invoke<Fn, Us..., T>;
    };

    template <typename Fn>
    struct negate_q
    {
        template <typename... Ts>
        using fn = std::bool_constant<!invoke_v<Fn, Ts...>>;
    };

    template <typename... Fns>
    struct compose_q
    {
        template <typename... Ts>
        using fn = invoke<id_q, Ts...>;
    };
    template <typename First, typename... Rest>
    struct compose_q<First, Rest...>
    {
        template <typename... Ts>
        using fn = invoke<compose_q<Rest...>, invoke<First, Ts...>>;
    };

    namespace detail
    {
        // clang-format off
        template <typename List, template <typename...> typename Fn>
        struct unpack_invoke_impl {};
        template <template <typename...> typename Templ, typename... Types, template <typename...> typename Fn>
        struct unpack_invoke_impl<Templ<Types...>, Fn> : std::type_identity<Fn<Types...>> {};
        template <typename List, template <typename...> typename Fn>
        struct unpack_invoke_values_impl {};
        template <template <auto...> typename Templ, auto... values, template <typename...> typename Fn>
        struct unpack_invoke_values_impl<Templ<values...>, Fn> : std::type_identity<Fn<value_tag_t<values>...>> {};
        // clang-format on
    } // namespace detail

    template <typename List, typename Fn = quote<type_list>>
    using unpack_invoke = typename detail::unpack_invoke_impl<List, Fn::template fn>::type;
    using unpack_invoke_q = quote<unpack_invoke>;
    template <typename List, typename Fn>
    inline constexpr auto unpack_invoke_v = detail::unpack_invoke_impl<List, Fn::template fn>::type::value;

    template <typename List, typename Fn = quote<type_list>>
    using unpack_invoke_values = typename detail::unpack_invoke_values_impl<List, Fn::template fn>::type;
    using unpack_invoke_values_q = quote<unpack_invoke_values>;
    template <typename List, typename Fn>
    inline constexpr auto unpack_invoke_values_v =
        detail::unpack_invoke_values_impl<List, Fn::template fn>::type::value;

    template <auto... values>
    using to_type_list = type_list<value_tag_t<values>...>;
    template <typename List>
    using to_type_list_l = unpack_invoke_values<List>;

    template <typename... Types>
    using to_value_list = value_list<Types::value...>;
    using to_value_list_q = quote<to_value_list>;
    template <typename List>
    using to_value_list_l = unpack_invoke<List, quote<to_value_list>>;

    // clang-format off
    template <template <typename...> typename Fn, typename... Args>
    concept valid = requires { typename invoke<quote<Fn>, Args...>; };
    template <template <typename> typename Fn, typename Arg>
    concept valid1 = requires { typename Fn<Arg>; };
    // clang-format on

    namespace detail
    {
        // clang-format off
        template <typename Fn, typename Default, typename... Args>
        struct invoke_if_valid_impl : std::type_identity_t<Default> {};
        template <typename Fn, typename Default, typename... Args>
            requires requires { typename invoke<Fn, Args...>; }
        struct invoke_if_valid_impl<Fn, Default, Args...> : std::type_identity_t<invoke<Fn, Args...>> {};
        // clang-format on
    } // namespace detail

    template <typename Fn, typename Default, typename... Args>
    using invoke_if_valid = detail::invoke_if_valid_impl<Fn, Default, Args...>;

    namespace detail
    {
        // clang-format off
        template <
            template <typename...> typename TemplT, typename... Ts,
            template <typename...> typename TemplU, typename... Us>
        type_list<Ts..., Us...> concat_impl(TemplT<Ts...>, TemplU<Us...>);
        // clang-format on

        template <template <typename...> typename Templ, typename... Ts, std::size_t... idx>
        type_list<indexed_type<idx, Ts>...> enumerate_impl(Templ<Ts...>, std::index_sequence<idx...>);
    } // namespace detail

    template <typename T>
    struct constant_q
    {
        template <typename... Ts>
        using fn = T;
    };
    template <typename List, typename T>
    using constant_l = unpack_invoke<List, constant_q<T>>;

    template <typename List1, typename List2>
    using concatenate_l = decltype(detail::concat_impl(List1{}, List2{}));

    template <typename... Ts>
    using list_size = std::integral_constant<std::size_t, sizeof...(Ts)>;
    using list_size_q = quote<list_size>;
    template <typename List>
    using list_size_l = unpack_invoke<List, list_size_q>;
    template <typename List>
    inline constexpr auto list_size_lv = unpack_invoke_v<List, list_size_q>;

    template <typename... Ts>
    using empty = std::bool_constant<sizeof...(Ts) == 0>;
    using empty_q = quote<empty>;
    template <typename List>
    using empty_l = unpack_invoke<List, empty_q>;
    template <typename List>
    inline constexpr bool empty_lv = unpack_invoke_v<List, empty_q>;

    template <typename... Ts>
    using enumerate = decltype(detail::enumerate_impl(type_list<Ts...>{}, std::make_index_sequence<sizeof...(Ts)>{}));
    using enumerate_q = quote<enumerate>;
    template <typename List>
    using enumerate_l = unpack_invoke<List, enumerate_q>;

    namespace detail
    {
        template <typename... Ts>
        struct nth_type_impl : Ts...
        {
        };

        template <std::size_t I, typename T>
        type_tag_t<T> nth_type_fn(indexed_type<I, T>);
    } // namespace detail

    template <std::size_t I>
    struct nth_type_q
    {
        template <typename... Ts>
        using fn = typename decltype(detail::nth_type_fn<I>(
            unpack_invoke<enumerate<Ts...>, quote<detail::nth_type_impl>>{}))::type;
    };
    template <typename List, std::size_t I>
    using nth_type_l = unpack_invoke<List, nth_type_q<I>>;

    template <typename T>
    struct push_back_q
    {
        template <typename... Ts>
        using fn = type_list<Ts..., T>;
    };
    template <typename List, typename T>
    using push_back_l = unpack_invoke<List, push_back_q<T>>;

    template <typename T>
    struct push_back_unique_q
    {
        template <typename... Ts>
        using fn = conditional_t<same_as_any_of<T, Ts...>, type_list<Ts...>, type_list<Ts..., T>>;
    };
    template <typename List, typename T>
    using push_back_unique_l = unpack_invoke<List, push_back_unique_q<T>>;

    template <typename T>
    struct push_front_q
    {
        template <typename... Ts>
        using fn = type_list<T, Ts...>;
    };
    template <typename List, typename T>
    using push_front_l = unpack_invoke<List, push_front_q<T>>;

    template <typename Target>
    struct contains_q
    {
        template <typename... Ts>
        using fn = std::bool_constant<same_as_any_of<Target, Ts...>>;
    };
    template <typename List, typename Target>
    using contains_l = unpack_invoke<List, contains_q<Target>>;
    template <typename List, typename Target>
    inline constexpr bool contains_lv = unpack_invoke_v<List, contains_q<Target>>;

    template <typename Pred = id_q>
    struct all_of_q
    {
        template <typename... Ts>
        static constexpr bool value = (invoke_v<Pred, Ts> && ...);
        template <typename... Ts>
        using fn = std::bool_constant<value<Ts...>>;
    };
    template <typename List, typename Pred = id_q>
    using all_of_l = unpack_invoke<List, all_of_q<Pred>>;
    template <typename List, typename Pred = id_q>
    inline constexpr bool all_of_lv = unpack_invoke_v<List, all_of_q<Pred>>;

    template <typename Pred = id_q>
    struct any_of_q
    {
        template <typename... Ts>
        static constexpr bool value = (invoke_v<Pred, Ts> || ...);
        template <typename... Ts>
        using fn = std::bool_constant<value<Ts...>>;
    };
    template <typename List, typename Pred = id_q>
    using any_of_l = unpack_invoke<List, any_of_q<Pred>>;
    template <typename List, typename Pred = id_q>
    inline constexpr bool any_of_lv = unpack_invoke_v<List, any_of_q<Pred>>;

    template <typename Pred = id_q>
    struct none_of_q
    {
        template <typename... Ts>
        static constexpr bool value = !(invoke_v<Pred, Ts> || ...);
        template <typename... Ts>
        using fn = std::bool_constant<value<Ts...>>;
    };
    template <typename List, typename Pred = id_q>
    using none_of_l = unpack_invoke<List, none_of_q<Pred>>;
    template <typename List, typename Pred = id_q>
    inline constexpr bool none_of_lv = unpack_invoke_v<List, none_of_q<Pred>>;

    // clang-format off
    template <typename Target>
    struct find_q
    {
        template <typename... Ts>
        struct fn : value_tag_t<npos> {};
    };
    template <typename Target>
    template <typename... Rest>
    struct find_q<Target>::fn<Target, Rest...> : std::integral_constant<size_t, 0> {};
    template <typename Target>
    template <typename First, typename... Rest>
    struct find_q<Target>::fn<First, Rest...> : detail::add_one_t<fn<Rest...>> {};
    // clang-format on
    template <typename List, typename Target>
    using find_l = unpack_invoke<List, find_q<Target>>;
    template <typename List, typename Target>
    inline constexpr auto find_lv = unpack_invoke_v<List, find_q<Target>>;

    template <typename Target>
    struct count_q
    {
        template <typename... Ts>
        static constexpr std::size_t value = (static_cast<std::size_t>(std::is_same_v<Target, Ts>) + ... + 0_uz);
        template <typename... Ts>
        using fn = std::integral_constant<std::size_t, value<Ts...>>;
    };
    template <typename List, typename Target>
    using count_l = unpack_invoke<List, count_q<Target>>;
    template <typename List, typename Target>
    inline constexpr auto count_lv = unpack_invoke_v<List, count_q<Target>>;

    template <typename Pred = id_q>
    struct count_if_q
    {
        template <typename... Ts>
        static constexpr std::size_t value = (static_cast<std::size_t>(invoke_v<Pred, Ts>) + ... + 0_uz);
        template <typename... Ts>
        using fn = std::integral_constant<std::size_t, value<Ts...>>;
    };
    template <typename List, typename Pred = id_q>
    using count_if_l = unpack_invoke<List, count_if_q<Pred>>;
    template <typename List, typename Pred = id_q>
    inline constexpr auto count_if_lv = unpack_invoke_v<List, count_if_q<Pred>>;

    namespace detail
    {
        // clang-format off
        template <typename BinaryFn, typename Res, typename... Ts>
        struct foldl_impl : std::type_identity<Res> {};
        template <typename BinaryFn, typename Res, typename First, typename... Rest>
        struct foldl_impl<BinaryFn, Res, First, Rest...> : foldl_impl<BinaryFn, invoke<BinaryFn, Res, First>, Rest...> {};
        // clang-format on
    } // namespace detail

    template <typename BinaryFn, typename Initial>
    struct foldl_q
    {
        template <typename... Ts>
        using fn = typename detail::foldl_impl<BinaryFn, Initial, Ts...>::type;
    };
    template <typename List, typename BinaryFn, typename Initial>
    using foldl_l = unpack_invoke<List, foldl_q<BinaryFn, Initial>>;

    template <typename... Lists>
    using flatten = foldl_q<quote<concatenate_l>, type_list<>>::fn<Lists...>;
    using flatten_q = quote<flatten>;
    template <typename Lists>
    using flatten_l = unpack_invoke<Lists, flatten_q>;

    template <typename UnaryFn>
    struct transform_q
    {
        template <typename... Ts>
        using fn = type_list<invoke<UnaryFn, Ts>...>;
    };
    template <typename List, typename UnaryFn>
    using transform_l = unpack_invoke<List, transform_q<UnaryFn>>;

    template <typename Old, typename New>
    struct replace_q
    {
        template <typename... Ts>
        using fn = type_list<conditional_t<std::is_same_v<Ts, Old>, New, Ts>...>;
    };
    template <typename List, typename Old, typename New>
    using replace_all_l = unpack_invoke<List, replace_q<Old, New>>;

    template <typename Pred = id_q>
    struct filter_q
    {
    private:
        // clang-format off
        template <typename... Ts>
        struct fn_impl : std::type_identity<type_list<>> {};
        // clang-format on

    public:
        template <typename... Ts>
        using fn = typename fn_impl<Ts...>::type;
    };
    template <typename Pred>
    template <typename First, typename... Rest>
    struct filter_q<Pred>::fn_impl<First, Rest...>
    {
        using rest_list = typename fn_impl<Rest...>::type;
        using type = conditional_t<invoke_v<Pred, First>, push_front_l<rest_list, First>, rest_list>;
    };
    template <typename List, typename Pred = id_q>
    using filter_l = unpack_invoke<List, filter_q<Pred>>;

    template <typename Target>
    struct remove_q
    {
    private:
        // clang-format off
        template <typename... Ts>
        struct fn_impl : std::type_identity<type_list<>> {};
        // clang-format on

    public:
        template <typename... Ts>
        using fn = typename fn_impl<Ts...>::type;
    };
    template <typename Target>
    template <typename First, typename... Rest>
    struct remove_q<Target>::fn_impl<First, Rest...>
    {
        using rest_list = typename fn_impl<Rest...>::type;
        using type = conditional_t<std::is_same_v<Target, First>, rest_list, push_front_l<rest_list, First>>;
    };
    template <typename List, typename Target>
    using remove_l = unpack_invoke<List, remove_q<Target>>;

    // clang-format off
    template <typename... Ts>
    struct is_unique : std::true_type {};
    template <typename First, typename... Rest>
    struct is_unique<First, Rest...>
        : std::bool_constant<!(std::is_same_v<First, Rest> || ...) && is_unique<Rest...>::value> {};
    // clang-format on
    using is_unique_q = quote<is_unique>;
    template <typename List>
    using is_unique_l = unpack_invoke<List, is_unique_q>;
    template <typename List>
    inline constexpr bool is_unique_lv = unpack_invoke_v<List, is_unique_q>;

    template <typename... Ts>
    using unique = foldl_q<quote<push_back_unique_l>, type_list<>>::fn<Ts...>;
    using unique_q = quote<unique>;
    template <typename List>
    using unique_l = unpack_invoke<List, unique_q>;

    template <typename Set1, typename Set2>
    using set_include_l = all_of_l<Set2, bind_front_q<quote<contains_l>, Set1>>;
    template <typename Set1, typename Set2>
    inline constexpr bool set_include_lv = set_include_l<Set1, Set2>::value;

    template <typename Set1, typename Set2>
    inline constexpr bool set_equal_lv = (list_size_lv<Set1> == list_size_lv<Set2>)&&set_include_lv<Set1, Set2>;
    template <typename Set1, typename Set2>
    using set_equal_l = std::bool_constant<set_equal_lv<Set1, Set2>>;

    template <typename Set1, typename Set2>
    using set_union_l = unique_l<concatenate_l<Set1, Set2>>;
    template <typename Set1, typename Set2>
    using set_intersection_l = filter_l<Set2, bind_front_q<quote<contains_l>, Set1>>;
    template <typename Set1, typename Set2>
    using set_difference_l = filter_l<Set1, negate_q<bind_front_q<quote<contains_l>, Set2>>>;
    template <typename Set1, typename Set2>
    using set_symmetric_difference_l = concatenate_l<set_difference_l<Set1, Set2>, set_difference_l<Set2, Set1>>;
} // namespace clu::meta
