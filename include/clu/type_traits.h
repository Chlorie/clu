#pragma once

#include <type_traits>

namespace clu
{
    template <typename T>
    struct type_tag_t
    {
        using type = T;
    };
    template <typename T>
    inline constexpr type_tag_t<T> type_tag{};

    template <typename From, typename To>
    struct copy_cvref
    {
    private:
        using removed = std::remove_cvref_t<To>;
        using cv_qual = std::remove_reference_t<From>;
        using cqual = std::conditional_t<std::is_const_v<cv_qual>, std::add_const_t<removed>, removed>;
        using vqual = std::conditional_t<std::is_volatile_v<cv_qual>, std::add_volatile_t<cqual>, cqual>;
        using lref = std::conditional_t<std::is_lvalue_reference_v<From>, std::add_lvalue_reference_t<vqual>, vqual>;
        using rref = std::conditional_t<std::is_rvalue_reference_v<From>, std::add_rvalue_reference_t<lref>, lref>;

    public:
        using type = rref;
    };

    template <typename From, typename To> using copy_cvref_t = typename copy_cvref<From, To>::type;

    template <typename... Ts> struct all_same : std::false_type {};
    template <> struct all_same<> : std::true_type {};
    template <typename T> struct all_same<T> : std::true_type {};
    template <typename T, typename... Rest> struct all_same<T, Rest...> : std::bool_constant<(std::is_same_v<T, Rest> && ...)> {};

    template <typename... Ts> inline constexpr bool all_same_v = all_same<Ts...>::value;

    template <typename T> inline constexpr bool no_cvref_v = std::is_same_v<T, std::remove_cvref_t<T>>;
    template <typename T> struct no_cvref : std::bool_constant<no_cvref_v<T>> {};
}
