#pragma once

#include <compare>

#include "type_traits.h"
#include "meta_list.h"
#include "concepts.h"

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

    template <template <typename...> typename Fn>
    struct quote
    {
        template <typename... Ts>
        using fn = Fn<Ts...>;
    };

    template <template <typename> typename UnaryFn>
    struct quote1
    {
        template <typename T>
        using fn = UnaryFn<T>;
    };

    template <template <typename, typename> typename BinaryFn>
    struct quote2
    {
        template <typename T, typename U>
        using fn = BinaryFn<T, U>;
    };

    template <typename T> using id = T;
    using id_q = quote1<id>;
    template <typename T> using _t = typename T::type;
    using _t_q = quote1<_t>;

    template <typename T> inline constexpr auto _v = T::value;

    namespace detail
    {
        template <typename Fn, typename... Ts>
        struct invoke_impl : std::type_identity<typename Fn::template fn<Ts...>> {};
        template <typename Fn, typename T>
        struct invoke_impl<Fn, T> : std::type_identity<typename Fn::template fn<T>> {};
        template <typename Fn, typename T, typename U>
        struct invoke_impl<Fn, T, U> : std::type_identity<typename Fn::template fn<T, U>> {};
    }

    template <typename Fn, typename... Ts>
    using invoke = typename detail::invoke_impl<Fn, Ts...>::type;
    template <typename Fn, typename... Ts>
    inline constexpr auto invoke_v = detail::invoke_impl<Fn, Ts...>::type::value;

    template <typename Pred, typename TrueTrans, typename FalseTrans = id_q>
    struct if_q
    {
        template <typename T>
        using fn = invoke<conditional_t<invoke<Pred, T>::value, TrueTrans, FalseTrans>, T>;
    };

    template <typename Fn, typename T>
    struct bind_front_q
    {
        template <typename... Us>
        using fn = invoke<Fn, T, Us...>;
    };

    template <typename Fn, typename T>
    struct bind_front_q1
    {
        template <typename U>
        using fn = invoke<Fn, T, U>;
    };

    template <typename Fn, typename T>
    struct bind_back_q
    {
        template <typename... Us>
        using fn = invoke<Fn, Us..., T>;
    };

    template <typename Fn, typename T>
    struct bind_back_q1
    {
        template <typename U>
        using fn = invoke<Fn, U, T>;
    };

    template <typename Fn>
    struct negate_q
    {
        template <typename... Ts>
        using fn = std::bool_constant<!invoke_v<Fn, Ts...>>;
    };

    template <typename Fn>
    struct negate_q1
    {
        template <typename T>
        using fn = std::bool_constant<!invoke_v<Fn, T>>;
    };

    template <typename... Fns>
    struct compose_q
    {
        template <typename T>
        using fn = T;
    };
    template <typename First, typename... Rest>
    struct compose_q<First, Rest...>
    {
        template <typename T>
        using fn = typename compose_q<Rest...>::template fn<typename First::template fn<T>>;
    };

    namespace detail
    {
        template <typename List, template <typename...> typename Fn>
        struct unpack_invoke_impl {};
        template <template <typename...> typename Templ, typename... Types, template <typename...> typename Fn>
        struct unpack_invoke_impl<Templ<Types...>, Fn> : std::type_identity<Fn<Types...>> {};
    }

    template <typename List, typename Fn = quote<type_list>>
    using unpack_invoke = typename detail::unpack_invoke_impl<List, Fn::template fn>::type;
    using unpack_invoke_q = quote2<unpack_invoke>;
    template <typename List, typename Fn = quote<type_list>>
    inline constexpr auto unpack_invoke_v = detail::unpack_invoke_impl<List, Fn::template fn>::type::value;

    template <template <typename...> typename Fn, typename... Args>
    concept valid = requires { typename Fn<Args...>; };
    template <template <typename> typename Fn, typename Arg>
    concept valid1 = requires { typename Fn<Arg>; };

    namespace detail
    {
        template <typename Fn, typename Default, typename... Args>
        struct invoke_if_valid_impl : std::type_identity_t<Default> {};
        template <typename Fn, typename Default, typename... Args>
            requires requires { typename invoke<Fn, Args...>; }
        struct invoke_if_valid_impl<Fn, Default, Args...> : std::type_identity_t<invoke<Fn, Args...>> {};
    }
    
    template <typename Fn, typename Default, typename... Args>
    using invoke_if_valid = detail::invoke_if_valid_impl<Fn, Default, Args...>;
    
    namespace detail
    {
        template <
            template <typename...> typename TemplT, typename... Ts,
            template <typename...> typename TemplU, typename... Us>
        type_list<Ts..., Us...> concat_impl(TemplT<Ts...>, TemplU<Us...>);

        template <template <typename...> typename Templ, typename... Ts, std::size_t... idx>
        type_list<indexed_type<idx, Ts>...> enumerate_impl(Templ<Ts...>, std::index_sequence<idx...>);
    }

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
    using enumerate = decltype(detail::enumerate_impl(
        type_list<Ts...>{}, std::make_index_sequence<sizeof...(Ts)>{}));
    using enumerate_q = quote<enumerate>;
    template <typename List>
    using enumerate_l = unpack_invoke<List, enumerate_q>;

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
        using fn = conditional_t<
            same_as_any_of<T, Ts...>,
            type_list<Ts...>,
            type_list<Ts..., T>>;
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
    private:
        template <typename... Ts> // MSVC 17.0 workaround
        static constexpr bool value = (invoke_v<Pred, Ts> && ...);
    public:
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
    private:
        template <typename... Ts> // MSVC 17.0 workaround
        static constexpr bool value = (invoke_v<Pred, Ts> || ...);
    public:
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
    private:
        template <typename... Ts> // MSVC 17.0 workaround
        static constexpr bool value = !(invoke_v<Pred, Ts> || ...);
    public:
        template <typename... Ts>
        using fn = std::bool_constant<value<Ts...>>;
    };
    template <typename List, typename Pred = id_q>
    using none_of_l = unpack_invoke<List, none_of_q<Pred>>;
    template <typename List, typename Pred = id_q>
    inline constexpr bool none_of_lv = unpack_invoke_v<List, none_of_q<Pred>>;

    template <typename Target>
    struct find_q
    {
        template <typename... Ts>
        struct fn : value_tag_t<npos> {};
    };
    template <typename Target> template <typename... Rest>
    struct find_q<Target>::fn<Target, Rest...> : std::integral_constant<size_t, 0> {};
    template <typename Target> template <typename First, typename... Rest>
    struct find_q<Target>::fn<First, Rest...> : detail::add_one_t<fn<Rest...>> {};
    template <typename List, typename Target>
    using find_l = unpack_invoke<List, find_q<Target>>;
    template <typename List, typename Target>
    inline constexpr auto find_lv = unpack_invoke_v<List, find_q<Target>>;

    template <typename Target>
    struct count_q
    {
    private:
        template <typename... Ts> // MSVC 17.0 workaround
        static constexpr std::size_t value = (static_cast<std::size_t>(std::is_same_v<Target, Ts>) + ... + 0_uz);
    public:
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
    private:
        template <typename... Ts> // MSVC 17.0 workaround
        static constexpr std::size_t value = (static_cast<std::size_t>(invoke_v<Pred, Ts>) + ... + 0_uz);
    public:
        template <typename... Ts>
        using fn = std::integral_constant<std::size_t, value<Ts...>>;
    };
    template <typename List, typename Pred = id_q>
    using count_if_l = unpack_invoke<List, count_if_q<Pred>>;
    template <typename List, typename Pred = id_q>
    inline constexpr auto count_if_lv = unpack_invoke_v<List, count_if_q<Pred>>;

    namespace detail
    {
        template <typename BinaryFn, typename Res, typename... Ts>
        struct foldl_impl : std::type_identity<Res> {};

        template <typename BinaryFn, typename Res, typename First, typename... Rest>
        struct foldl_impl<BinaryFn, Res, First, Rest...> :
            foldl_impl<BinaryFn, invoke<BinaryFn, Res, First>, Rest...> {};
    }

    template <typename BinaryFn, typename Initial>
    struct foldl_q
    {
        template <typename... Ts>
        using fn = typename detail::foldl_impl<BinaryFn, Initial, Ts...>::type;
    };
    template <typename List, typename BinaryFn, typename Initial>
    using foldl_l = unpack_invoke<List, foldl_q<BinaryFn, Initial>>;

    template <typename... Lists>
    using flatten = foldl_q<quote2<concatenate_l>, type_list<>>::fn<Lists...>;
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
        template <typename... Ts>
        struct fn_impl : std::type_identity<type_list<>> {};
    public:
        template <typename... Ts>
        using fn = typename fn_impl<Ts...>::type;
    };
    template <typename Pred>
    template <typename First, typename... Rest>
    struct filter_q<Pred>::fn_impl<First, Rest...>
    {
        using rest_list = typename fn_impl<Rest...>::type;
        using type = conditional_t<
            invoke_v<Pred, First>,
            push_front_l<rest_list, First>,
            rest_list
        >;
    };
    template <typename List, typename Pred = id_q>
    using filter_l = unpack_invoke<List, filter_q<Pred>>;

    template <typename Target>
    using remove_q = filter_q<
        compose_q<
            bind_front_q1<quote2<std::is_same>, Target>,
            quote1<std::negation>
        >
    >;
    template <typename List, typename Target>
    using remove_l = unpack_invoke<List, remove_q<Target>>;

    template <typename... Ts>
    struct is_unique : std::true_type {};
    template <typename First, typename... Rest>
    struct is_unique<First, Rest...> :
        std::bool_constant<!(std::is_same_v<First, Rest> || ...) && is_unique<Rest...>::value> {};
    using is_unique_q = quote<is_unique>;
    template <typename List>
    using is_unique_l = unpack_invoke<List, is_unique_q>;
    template <typename List>
    inline constexpr bool is_unique_lv = unpack_invoke_v<List, is_unique_q>;

    template <typename... Ts>
    using unique = foldl_q<quote2<push_back_unique_l>, type_list<>>::fn<Ts...>;
    using unique_q = quote<unique>;
    template <typename List>
    using unique_l = unpack_invoke<List, unique_q>;

    template <typename Set1, typename Set2>
    using set_include_l = all_of_l<Set2, bind_front_q1<quote2<contains_l>, Set1>>;
    template <typename Set1, typename Set2>
    inline constexpr bool set_include_lv = set_include_l<Set1, Set2>::value;

    template <typename Set1, typename Set2>
    inline constexpr bool set_equal_lv = Set1::size == Set2::size && set_include_lv<Set1, Set2>;
    template <typename Set1, typename Set2>
    using set_equal_l = std::bool_constant<set_equal_lv<Set1, Set2>>;

    template <typename Set1, typename Set2>
    using set_union_l = unique_l<concatenate_l<Set1, Set2>>;
    template <typename Set1, typename Set2>
    using set_intersection_l = filter_l<Set2, bind_front_q1<quote2<contains_l>, Set1>>;
    template <typename Set1, typename Set2>
    using set_difference_l = filter_l<Set1, negate_q<bind_front_q1<quote2<contains_l>, Set2>>>;
    template <typename Set1, typename Set2>
    using set_symmetric_difference_l = concatenate_l<set_difference_l<Set1, Set2>, set_difference_l<Set2, Set1>>;
}
