#pragma once

#include <tuple>
#include <memory>

#include "concepts.h"
#include "meta_algorithm.h"

namespace clu
{
    template <typename S, typename I>
    concept nothrow_sentinel_for = std::sentinel_for<S, I>;

    template <typename I>
    concept nothrow_input_iterator = //
        std::input_iterator<I> && //
        std::is_lvalue_reference_v<std::iter_reference_t<I>> && //
        std::same_as<std::remove_cvref_t<std::iter_reference_t<I>>, std::iter_value_t<I>>;

    template <typename I>
    concept nothrow_forward_iterator = //
        nothrow_input_iterator<I> && //
        std::forward_iterator<I> && //
        nothrow_sentinel_for<I, I>;

    template <allocator Alloc, typename T>
    using rebound_allocator_t = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

    namespace detail
    {
        template <typename T, typename U>
        void deduce_as_pair(const std::pair<T, U>&);

        template <typename T, typename Tuple>
        inline constexpr bool is_nothrow_constructible_with_tuple =
            meta::unpack_invoke_v<Tuple, meta::bind_front_q<meta::quote<std::is_nothrow_constructible>, T>>;

        struct uses_alloc_ctargs
        {
            template <typename T, typename Alloc, typename... Args>
                requires(!template_of<T, std::pair>)
            constexpr static auto fn(const Alloc& alloc, Args&&... args) noexcept
            {
                if constexpr (!std::uses_allocator_v<T, Alloc>)
                {
                    if constexpr (std::is_constructible_v<T, Args...>)
                        return std::forward_as_tuple(static_cast<Args&&>(args)...);
                }
                else if constexpr (std::uses_allocator_v<T, Alloc>)
                {
                    if constexpr (std::is_constructible_v<T, std::allocator_arg_t, const Alloc&, Args...>)
                        return std::tuple<std::allocator_arg_t, const Alloc&, Args&&...>(
                            std::allocator_arg, alloc, static_cast<Args&&>(args)...);
                    else if constexpr (std::is_constructible_v<T, Args..., const Alloc&>)
                        return std::forward_as_tuple(static_cast<Args&&>(args)..., alloc);
                }
            }

            template <template_of<std::pair> T, typename Alloc, typename Tuple1, typename Tuple2>
            constexpr static auto fn(const Alloc& alloc, std::piecewise_construct_t, Tuple1&& t1, Tuple2&& t2) noexcept
            {
                if constexpr (valid_tuple<typename T::first_type, Alloc, Tuple1> &&
                    valid_tuple<typename T::second_type, Alloc, Tuple2>)
                    return std::make_tuple(std::piecewise_construct,
                        uses_alloc_ctargs::fn_with_tuple<typename T::first_type>(alloc, static_cast<Tuple1>(t1)),
                        uses_alloc_ctargs::fn_with_tuple<typename T::second_type>(alloc, static_cast<Tuple2>(t2)));
            }

            template <template_of<std::pair> T, typename Alloc>
            constexpr static auto fn(const Alloc& alloc) noexcept
            {
                return uses_alloc_ctargs::fn<T>(alloc, std::piecewise_construct, std::tuple{}, std::tuple{});
            }

            template <template_of<std::pair> T, typename Alloc, typename U, typename V>
            constexpr static auto fn(const Alloc& alloc, U&& first, V&& second) noexcept
            {
                return uses_alloc_ctargs::fn<T>(alloc, std::piecewise_construct,
                    std::tuple<U&&>{static_cast<U&&>(first)}, std::tuple<V&&>{static_cast<V&&>(second)});
            }

            template <template_of<std::pair> T, typename Alloc, typename U>
            constexpr static auto fn(const Alloc& alloc, U&& arg) noexcept
            {
                if constexpr (is_template_of_v<std::remove_cvref_t<U>, std::pair>)
                {
                    return uses_alloc_ctargs::fn<T>(alloc, std::piecewise_construct,
                        std::forward_as_tuple(std::get<0>(static_cast<U&&>(arg))),
                        std::forward_as_tuple(std::get<1>(static_cast<U&&>(arg))));
                }
                else if constexpr (!requires { detail::deduce_as_pair(static_cast<U&&>(arg)); })
                {
                    using no_cv = std::remove_cv_t<T>;
                    // This not equal is actually xor for bools, we proceed only if exactly
                    // one conversion succeeds, since if both conversions succeed it is an ambiguity.
                    if constexpr (std::is_convertible_v<U, const no_cv&> != std::is_convertible_v<U, no_cv&&>)
                    {
                        using target = conditional_t<std::is_convertible_v<U, const no_cv&>, const no_cv&, no_cv&&>;
                        using ctargs_tuple = decltype(uses_alloc_ctargs::fn<no_cv>(alloc, std::declval<target>()));

                        class converter
                        {
                        public:
                            constexpr converter(const Alloc& a, U& u) noexcept: alloc_(a), arg_(u) {}

                            constexpr operator no_cv() const noexcept( //
                                is_nothrow_constructible_with_tuple<no_cv, ctargs_tuple>&& //
                                    std::is_nothrow_convertible_v<U, target>)
                            {
                                return std::make_from_tuple<no_cv>(
                                    uses_alloc_ctargs::fn<no_cv>(alloc_, static_cast<target>(static_cast<U&&>(arg_))));
                            }

                        private:
                            const Alloc& alloc_;
                            U& arg_;
                        };
                        return std::tuple{converter(alloc, arg)};
                    }
                }
            }

            template <typename T, typename Alloc, typename Tuple>
            constexpr static auto fn_with_tuple(const Alloc& alloc, Tuple&& tuple) noexcept
            {
                return std::apply([&]<typename... Ts>(Ts&&... args)
                    { return uses_alloc_ctargs::fn<T>(alloc, static_cast<Ts&&>(args)...); },
                    static_cast<Tuple&&>(tuple));
            }

            template <typename T, typename Alloc, typename... Args>
            using tuple_t = decltype(uses_alloc_ctargs::fn<T>(std::declval<const Alloc&>(), std::declval<Args>()...));
            template <typename T, typename Alloc, typename... Args>
            constexpr static bool valid = !std::is_void_v<tuple_t<T, Alloc, Args...>>;
            template <typename T, typename Alloc, typename Tuple>
            constexpr static bool valid_tuple = !std::is_void_v<decltype(uses_alloc_ctargs::fn_with_tuple<T>(
                std::declval<const Alloc&>(), std::declval<Tuple>()...))>;
        };
    } // namespace detail

    template <typename T, typename Alloc>
    concept uses_allocator = allocator<Alloc> && std::uses_allocator_v<T, Alloc>;

    template <typename T, typename Alloc, typename... Args>
    concept constructible_using_allocator_from =
        allocator<Alloc> && detail::uses_alloc_ctargs::valid<T, Alloc, Args...>;

    template <typename T, typename Alloc, typename... Args>
        requires constructible_using_allocator_from<T, Alloc, Args...>
    constexpr auto uses_allocator_construction_args(const Alloc& alloc, Args&&... args) noexcept
    {
        return detail::uses_alloc_ctargs::fn<T>(alloc, static_cast<Args&&>(args)...);
    }

    template <typename T, typename Alloc, typename... Args>
        requires constructible_using_allocator_from<T, Alloc, Args...>
    using uses_allocator_construction_args_type_t = detail::uses_alloc_ctargs::tuple_t<T, Alloc, Args...>;

    template <typename T, typename Alloc, typename... Args>
    concept nothrow_constructible_using_allocator_from = //
        constructible_using_allocator_from<T, Alloc, Args...> &&
        detail::is_nothrow_constructible_with_tuple<T, //
            uses_allocator_construction_args_type_t<T, Alloc, Args...>>;

    template <typename T, allocator Alloc, typename... Args>
    auto new_object_using_allocator(const Alloc& alloc, Args&&... args)
    {
        using rebound = rebound_allocator_t<Alloc, T>;
        using traits = std::allocator_traits<rebound>;
        rebound al(alloc);
        auto pointer = traits::allocate(al, 1);
        scope_fail guard{[&] { traits::deallocate(al, pointer, 1); }};
        traits::construct(al, std::to_address(pointer), static_cast<Args&&>(args)...);
        return pointer;
    }

    template <detail::allocator_ptr Ptr, allocator Alloc>
    void delete_object_using_allocator(const Alloc& alloc, Ptr pointer) noexcept
    {
        using T = std::iter_value_t<Ptr>;
        using rebound = rebound_allocator_t<Alloc, T>;
        using traits = std::allocator_traits<rebound>;
        rebound al(alloc);
        traits::destroy(al, std::to_address(pointer));
        traits::deallocate(al, pointer, 1);
    }

    template <nothrow_forward_iterator I, allocator_for<std::iter_value_t<I>> Alloc>
        requires constructible_using_allocator_from<std::iter_value_t<I>, Alloc>
    I uninitialized_value_construct_n_using_allocator(I first, std::iter_difference_t<I> n, Alloc alloc)
    {
        using traits = std::allocator_traits<Alloc>;
        I rollback = first;
        scope_fail guard{[&]
            {
                for (; rollback != first; ++rollback)
                    traits::destroy(alloc, std::addressof(*rollback));
            }};
        for (; n-- > 0; ++first)
            traits::construct(alloc, std::to_address(first));
        return first;
    }

    template <nothrow_input_iterator I, allocator_for<std::iter_value_t<I>> Alloc>
        requires std::destructible<std::iter_value_t<I>>
    constexpr I destroy_n_using_allocator(I first, std::iter_difference_t<I> n, Alloc alloc) noexcept
    {
        for (; n != 0; (void)++first, --n)
            std::allocator_traits<Alloc>::destroy(alloc, std::addressof(*first));
        return first;
    }
} // namespace clu
