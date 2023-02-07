#pragma once

#include <concepts>
#include <ranges>

#include "type_traits.h"

namespace clu
{
    template <typename T, typename U>
    concept similar_to = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    // clang-format off
    template <typename Inv, typename Res, typename... Ts>
    concept invocable_of = std::invocable<Inv, Ts...> && std::convertible_to<std::invoke_result_t<Inv, Ts...>, Res>;
    template <typename F, typename... Args>
    concept nothrow_invocable =
        std::invocable<F, Args...> &&
        std::is_nothrow_invocable_v<F, Args...>;

    template <typename F, typename... Args>
    concept callable =
        requires(F func, Args... args) { static_cast<F&&>(func)(static_cast<Args&&>(args)...); };
    template <typename F, typename... Args>
    concept nothrow_callable =
        requires(F func, Args... args) { { static_cast<F&&>(func)(static_cast<Args&&>(args)...) } noexcept; };

    template <typename F, typename... Args>
    using call_result_t = decltype(std::declval<F>()(std::declval<Args>()...));
    template <typename F, typename... Args>
    struct call_result : std::type_identity<call_result_t<F, Args...>> {};

    // Some exposition-only concepts in the standard, good to have them here
    template <typename B>
    concept boolean_testable =
        std::convertible_to<B, bool> &&
        requires(B&& b) { { !static_cast<B&&>(b) } -> std::convertible_to<bool>; };

    template <typename T, typename U>
    concept weakly_equality_comparable_with =
        requires(const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u)
        {
            { t == u } -> boolean_testable;
            { t != u } -> boolean_testable;
            { u == t } -> boolean_testable;
            { u != t } -> boolean_testable;
        };

    template <typename T, typename U>
    concept partial_ordered_with =
        requires(const std::remove_reference_t<T>& t, const std::remove_reference_t<U>& u)
        {
            { t <  u } -> boolean_testable;
            { t >  u } -> boolean_testable;
            { t <= u } -> boolean_testable;
            { t <= u } -> boolean_testable;
            { u <  t } -> boolean_testable;
            { u >  t } -> boolean_testable;
            { u <= t } -> boolean_testable;
            { u <= t } -> boolean_testable;
        };

    template <typename T>
    concept movable_value = 
        std::move_constructible<std::decay_t<T>> &&
        std::constructible_from<std::decay_t<T>, T>;

    template <typename From, typename To>
    concept decays_to = std::same_as<std::decay_t<From>, To>;

    template <typename T>
    concept class_type = decays_to<T, T> && std::is_class_v<T>;

    template <typename T>
    concept nullable_pointer =
        std::default_initializable<T> &&
        std::copy_constructible<T> &&
        std::is_copy_assignable_v<T> &&
        std::equality_comparable<T> &&
        weakly_equality_comparable_with<T, std::nullptr_t>;

    template <typename Type, template <typename...> typename Templ>
    struct is_template_of : std::false_type {};
    template <template <typename...> typename Templ, typename... Types>
    struct is_template_of<Templ<Types...>, Templ> : std::true_type {};
    template <typename Type, template <typename...> typename Templ>
    constexpr bool is_template_of_v = is_template_of<Type, Templ>::value;
    template <typename Type, template <typename...> typename Templ>
    concept template_of = is_template_of<Type, Templ>::value;
    // clang-format on

    template <typename T, typename... Us>
    concept same_as_any_of = (std::same_as<T, Us> || ...);
    template <typename T, typename... Us>
    concept same_as_none_of = (!same_as_any_of<T, Us...>);

    // clang-format off
    template <typename T, typename U>
    concept forwarding = 
        (!std::is_rvalue_reference_v<T>) &&
        std::same_as<std::remove_cvref_t<U>, U> &&
        std::same_as<std::remove_cvref_t<T>, U>;
    // clang-format on

    template <typename T>
    concept enumeration = std::is_enum_v<T>;

    template <typename T>
    concept arithmetic = std::integral<T> || std::floating_point<T>;

    template <typename T>
    concept trivially_copyable = std::copyable<T> && std::is_trivially_copyable_v<T>;

    // clang-format off
    template <typename T, typename... Us>
    concept implicitly_constructible_from =
        std::constructible_from<T, Us...> &&
        requires(Us&&... args, void (*fptr)(const T&)) { fptr({static_cast<Us&&>(args)...}); };
    // clang-format on

    namespace detail
    {
        // clang-format off
        template <typename Ptr>
        concept allocator_ptr = nullable_pointer<Ptr> && std::contiguous_iterator<Ptr>;

        template <typename T> struct pointer_of : std::type_identity<typename T::value_type*> {};
        template <typename T> requires requires { typename T::pointer; }
        struct pointer_of<T> : std::type_identity<typename T::pointer> {};
        template <typename T> using pointer_of_t = typename pointer_of<T>::type;

        template <typename Ptr> struct diff_type_of : std::type_identity<std::ptrdiff_t> {};
        template <typename Ptr> requires requires { typename Ptr::difference_type; }
        struct diff_type_of<Ptr> : std::type_identity<typename Ptr::difference_type> {};
        template <typename Ptr>
        struct size_type_of : std::type_identity<std::make_unsigned_t<typename diff_type_of<Ptr>::type>> {};
        template <typename Ptr> requires requires { typename Ptr::size_type; }
        struct size_type_of<Ptr> : std::type_identity<typename Ptr::size_type> {};

        template <typename Ptr> struct specialization_first_arg {};
        template <template <typename...> typename Templ, typename T, typename... Args>
        struct specialization_first_arg<Templ<T, Args...>> : std::type_identity<T> {};
        // clang-format on

        template <typename Ptr>
        constexpr auto element_type_of_impl() noexcept
        {
            if constexpr (requires { typename Ptr::element_type; })
                return type_tag<typename Ptr::element_type>;
            else if constexpr (requires { typename specialization_first_arg<Ptr>::type; })
                return type_tag<typename specialization_first_arg<Ptr>::type>;
            else
                return;
        }

        template <typename Ptr>
        using element_type_of_t = typename decltype(element_type_of_impl<Ptr>())::type;

        template <typename Alloc>
        using size_type_of_allocator = typename size_type_of<pointer_of_t<Alloc>>::type;

        // clang-format off
        template <typename T> using always_eq = typename T::is_always_eq;
        template <typename T> using pocca = typename T::propagate_on_container_copy_assignment;
        template <typename T> using pocma = typename T::propagate_on_container_move_assignment;
        template <typename T> using pocs = typename T::propagate_on_container_swap;

        template <typename T>
        concept boolean_alias =
            std::same_as<T, std::true_type> ||
            std::same_as<T, std::false_type> ||
            std::derived_from<T, std::true_type> ||
            std::derived_from<T, std::false_type>;

        template <typename T>
        concept allocator_base =
            std::copy_constructible<T> &&
            std::equality_comparable<T> &&
            requires { typename T::value_type; } &&
            requires (
                T& alloc,
                pointer_of_t<T> ptr,
                size_type_of_allocator<T> size)
            {
                { alloc.allocate(size) } -> std::same_as<pointer_of_t<T>>;
                alloc.deallocate(ptr, size);
            };

        template <typename T>
        concept has_valid_allocator_types =
            allocator_ptr<typename std::allocator_traits<T>::pointer> &&
            allocator_ptr<typename std::allocator_traits<T>::const_pointer> &&
            std::convertible_to<typename std::allocator_traits<T>::pointer,
                                typename std::allocator_traits<T>::const_pointer> &&
            nullable_pointer<typename std::allocator_traits<T>::void_pointer> &&
            std::convertible_to<typename std::allocator_traits<T>::pointer,
                                typename std::allocator_traits<T>::void_pointer> &&
            nullable_pointer<typename std::allocator_traits<T>::const_void_pointer> &&
            std::convertible_to<typename std::allocator_traits<T>::pointer,
                                typename std::allocator_traits<T>::const_void_pointer> &&
            std::convertible_to<typename std::allocator_traits<T>::const_pointer,
                                typename std::allocator_traits<T>::const_void_pointer> &&
            std::convertible_to<typename std::allocator_traits<T>::void_pointer,
                                typename std::allocator_traits<T>::const_void_pointer> &&
            std::unsigned_integral<typename std::allocator_traits<T>::size_type> &&
            std::signed_integral<typename std::allocator_traits<T>::difference_type>;

        template <typename Ptr>
        concept traits_has_pointer_to =
            std::is_pointer_v<Ptr> ||
            (
                requires { typename element_type_of_t<Ptr>; } &&
                requires(element_type_of_t<Ptr>& ref)
                {
                    { Ptr::pointer_to(ref) } -> std::same_as<Ptr>;
                }
            );

        template <typename T>
        concept has_valid_allocator_ptr_operations =
            requires (
                typename std::allocator_traits<T>::pointer p,
                typename std::allocator_traits<T>::const_pointer cp,
                typename std::allocator_traits<T>::void_pointer vp,
                typename std::allocator_traits<T>::const_void_pointer cvp)
            {
                { *p } -> std::same_as<typename T::value_type&>;
                { *cp } -> std::same_as<const typename T::value_type&>;
                static_cast<typename std::allocator_traits<T>::pointer>(vp);
                static_cast<typename std::allocator_traits<T>::const_pointer>(cvp);
            } &&
            traits_has_pointer_to<typename std::allocator_traits<T>::pointer> &&
            requires (typename T::value_type& r)
            {
                { std::pointer_traits<typename std::allocator_traits<T>::pointer>::pointer_to(r) }
                -> std::same_as<typename std::allocator_traits<T>::pointer>;
            };
            
        template <typename T>
        concept has_valid_soccc = 
            (!requires(T al) { al.select_on_container_copy_construction(); }) ||
            requires(T al)
            {
                { al.select_on_container_copy_construction() }
                -> std::same_as<T>;
            };

        template <typename T, template <typename> typename Member>
        concept valid_boolean_alias =
            (!requires { typename Member<T>; }) ||
            boolean_alias<Member<T>>;

        template <typename T>
        concept has_valid_allocator_boolean_aliases =
            valid_boolean_alias<T, always_eq> &&
            valid_boolean_alias<T, pocca> &&
            valid_boolean_alias<T, pocma> &&
            valid_boolean_alias<T, pocs>;
        // clang-format on
    } // namespace detail

    // clang-format off
    template <typename T>
    concept allocator =
        detail::allocator_base<T> &&
        detail::has_valid_allocator_types<T> &&
        detail::has_valid_allocator_ptr_operations<T> &&
        detail::has_valid_soccc<T> &&
        detail::has_valid_allocator_boolean_aliases<T>;

    template <typename L>
    concept basic_lockable = requires(L m)
    {
        m.lock();
        { m.unlock() } noexcept;
    };

    template <typename L>
    concept lockable =
        basic_lockable<L> &&
        requires(L m)
        {
            { m.try_lock() } -> boolean_testable;
        };

    template <typename L>
    concept shared_lockable = requires(L m)
    {
        m.lock_shared();
        { m.try_lock_shared() } -> boolean_testable;
        { m.unlock_shared() } noexcept;
    };

    template <typename M>
    concept mutex_like =
        std::default_initializable<M> &&
        std::destructible<M> &&
        (!std::copyable<M>) &&
        (!std::movable<M>) &&
        lockable<M>;

    template <typename M>
    concept shared_mutex_like =
        mutex_like<M> &&
        shared_lockable<M>;
    // clang-format on
} // namespace clu
