module;
#include <type_traits>
#include <compare>
#include "../macros.h"

export module clu.core:type_traits;

namespace clu
{
    export
    {
        template <typename Type>
        struct type_tag_t
        {
            using type = Type;
        };
        template <typename Type>
        inline constexpr type_tag_t<Type> type_tag{};

        template <auto val>
        struct value_tag_t
        {
            using type = value_tag_t;
            using value_type = decltype(val);
            static constexpr auto value = val;
        };
        template <auto val>
        inline constexpr value_tag_t<val> value_tag{};

        struct monostate
        {
            friend constexpr bool operator==(monostate, monostate) noexcept = default;
            friend constexpr std::strong_ordering operator<=>(monostate, monostate) noexcept = default;
        };

        struct unit
        {
            friend constexpr bool operator==(unit, unit) noexcept = default;
        };

        // clang-format off
        template <std::size_t I> struct priority_tag : priority_tag<I - 1> {};
        template <> struct priority_tag<0> {};
        // clang-format on
    }

    namespace detail
    {
        template <bool value>
        struct conditional_impl
        {
            template <typename True, typename>
            using type = True;
        };

        template <>
        struct conditional_impl<false>
        {
            template <typename, typename False>
            using type = False;
        };
    } // namespace detail

    export
    {
        template <bool value, typename True, typename False>
        using conditional_t = typename detail::conditional_impl<value>::template type<True, False>;

        template <typename... Ts>
        inline constexpr bool dependent_false = false;

        template <typename From, typename To>
        struct copy_cvref
        {
        private:
            using removed = std::remove_cvref_t<To>;
            using cv_qual = std::remove_reference_t<From>;
            using cqual = std::conditional_t<std::is_const_v<cv_qual>, std::add_const_t<removed>, removed>;
            using vqual = std::conditional_t<std::is_volatile_v<cv_qual>, std::add_volatile_t<cqual>, cqual>;
            using lref =
                std::conditional_t<std::is_lvalue_reference_v<From>, std::add_lvalue_reference_t<vqual>, vqual>;
            using rref = std::conditional_t<std::is_rvalue_reference_v<From>, std::add_rvalue_reference_t<lref>, lref>;

        public:
            using type = rref;
        };

        template <typename From, typename To>
        using copy_cvref_t = typename copy_cvref<From, To>::type;

        template <typename Like, typename T>
        auto&& forward_like(T && value) noexcept
        {
            return static_cast<copy_cvref_t<Like, T>>(value);
        }

        template <typename T>
        using with_regular_void_t = conditional_t<std::is_void_v<T>, copy_cvref_t<T, unit>, T>;

        template <typename T, typename... Us>
        inline constexpr bool is_implicitly_constructible_from_v =
            requires(Us&&... args, void (*fptr)(const T&)) { fptr({static_cast<Us&&>(args)...}); };

        template <typename From, typename To>
        inline constexpr bool is_implicitly_convertible_to_v =
            requires(From&& from, void (*fptr)(const To&)) { fptr(static_cast<From&&>(from)); };

        template <typename From, typename To>
        inline constexpr bool is_narrowing_conversion_v = //
            is_implicitly_convertible_to_v<From, To> && !requires(From f) { To{f}; };

        // clang-format off
        template <typename... Ts> struct single_type {};
        template <typename T> struct single_type<T> : std::type_identity<T> {};
        template <typename... Ts> using single_type_t = typename single_type<Ts...>::type;

        template <typename... Ts> struct all_same : std::false_type {};
        template <> struct all_same<> : std::true_type {};
        template <typename T> struct all_same<T> : std::true_type {};
        template <typename T, typename... Rest>
        struct all_same<T, Rest...> : std::bool_constant<(std::is_same_v<T, Rest> && ...)> {};
        // clang-format on

        template <typename... Ts>
        inline constexpr bool all_same_v = all_same<Ts...>::value;

        template <typename T>
        inline constexpr bool no_cvref_v = std::is_same_v<T, std::remove_cvref_t<T>>;
        template <typename T>
        struct no_cvref : std::bool_constant<no_cvref_v<T>>
        {
        };
    }
} // namespace clu

// WORKAROUND: If this is not exported, anything related to std::format breaks
#if defined(CLU_MSVC) && _MSC_FULL_VER <= 193833130
    #pragma warning(push)
    #pragma warning(disable : 4455)
export namespace std
{
    using std::strong_ordering;
    using std::weak_ordering;
    using std::partial_ordering;
} // namespace std
    #pragma warning(pop)
#endif
