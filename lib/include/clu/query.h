#pragma once

#include <tuple>

#include "tag_invoke.h"
#include "meta_algorithm.h"
#include "macros.h"

namespace clu
{
    template <typename T>
    concept queryable = std::destructible<T>;

    struct empty_env
    {
    };

    namespace detail
    {
        struct get_env_t
        {
            template <typename T>
            CLU_STATIC_CALL_OPERATOR(decltype(auto))
            (const T& value) noexcept
            {
                if constexpr (tag_invocable<get_env_t, T>)
                {
                    static_assert(queryable<tag_invoke_result_t<get_env_t, T>>, "get_env should return a queryable");
                    return tag_invoke(get_env_t{}, value);
                }
                else
                    return empty_env{};
            }
        };

        struct forwarding_query_t
        {
            template <typename Qry>
            constexpr CLU_STATIC_CALL_OPERATOR(bool)(Qry) noexcept
            {
                if constexpr (tag_invocable<forwarding_query_t, Qry>)
                {
                    static_assert(
                        nothrow_tag_invocable<forwarding_query_t, Qry>, "forwarding_query should be noexcept");
                    static_assert(boolean_testable<tag_invoke_result_t<forwarding_query_t, Qry>>);
                    return tag_invoke(forwarding_query_t{}, Qry{}) ? true : false;
                }
                else
                    return std::derived_from<Qry, forwarding_query_t>;
            }
        };

        // clang-format off
        template <typename Qry>
        concept fwd_query = forwarding_query_t{}(Qry{});
        // clang-format on
    } // namespace detail

    using detail::forwarding_query_t;
    using detail::get_env_t;
    inline constexpr forwarding_query_t forwarding_query{};
    inline constexpr get_env_t get_env{};

    // clang-format off
    template <typename T>
    concept environment_provider =
        requires(const std::remove_cvref_t<T>& prov) { { get_env(prov) } -> queryable; };
    // clang-format on

    template <typename T>
    using env_of_t = call_result_t<get_env_t, T>;

    namespace detail
    {
        template <typename Qry, typename T>
        struct query_value
        {
            using query_type = Qry;
            using type = T;
            CLU_NO_UNIQUE_ADDRESS query_type query;
            CLU_NO_UNIQUE_ADDRESS type value;
        };

        template <typename Q>
        using is_fwd_qv = std::bool_constant<fwd_query<typename Q::query_type>>;

        template <typename... Qs>
        struct is_not_superseded_qv_q
        {
            template <typename Q>
            using fn = std::bool_constant<!(std::is_same_v<typename Q::query_type, typename Qs::query_type> || ...)>;
        };

        template <typename... Qs, typename... NewQs>
        constexpr auto readapted_qvs_impl(type_list<Qs...>, type_list<NewQs...>)
        {
            using old_fwd_qvs = meta::filter_q<meta::quote<is_fwd_qv>>::fn<Qs...>;
            using remaining_old_fwd_qvs = meta::filter_l<old_fwd_qvs, is_not_superseded_qv_q<NewQs...>>;
            return meta::concatenate_l<remaining_old_fwd_qvs, type_list<NewQs...>>{};
        }

        // clang-format off
        template <typename Qv, typename QvRef, typename... NewQvRefs>
        using qv_nothrow_constructible = std::bool_constant<
            (std::is_same_v<Qv, std::remove_cvref_t<NewQvRefs>> || ...)
                ? ((std::is_same_v<Qv, std::remove_cvref<NewQvRefs>>
                    ? std::is_nothrow_constructible_v<Qv, NewQvRefs> : true) && ...)
                : std::is_nothrow_constructible_v<Qv, QvRef>>;
        // clang-format on

        template <typename Env, typename... Qs>
        class adapted_env_t_
        {
        public:
            using adapted_queries = type_list<typename Qs::query_type...>;
            using adapted_query_values = type_list<Qs...>;

            // clang-format off
            template <typename Env2, typename... Ts>
                requires (!clu::template_of<std::remove_cvref_t<Env2>, adapted_env_t_>)
            constexpr explicit adapted_env_t_(Env2&& base, Ts&&... values) noexcept(
                std::is_nothrow_constructible_v<Env, Env2> &&
                (std::is_nothrow_constructible_v<typename Qs::type, Ts> && ...)):
                base_(static_cast<Env2&&>(base)),
                values_(static_cast<Ts&&>(values)...) {}
            // clang-format on

        private:
            template <typename... AllQs>
            constexpr static auto readapted_env_impl(type_list<AllQs...>)
            {
                return type_tag<adapted_env_t_<Env, AllQs...>>;
            }

            template <typename... NewQs>
            using readapted_env_t = typename decltype(adapted_env_t_::readapted_env_impl(
                detail::readapted_qvs_impl(type_list<Qs...>{}, type_list<NewQs...>{})))::type;

            template <typename Self, typename... AllQs, typename... NewQs>
            constexpr static bool is_nothrow_readaptable(type_list<AllQs...>, type_list<NewQs...>) noexcept
            {
                return (qv_nothrow_constructible<AllQs, copy_cvref_t<Self, AllQs>, NewQs...>::value && ...) &&
                    std::is_nothrow_constructible_v<Env, copy_cvref_t<Self, Env>>;
            }

            template <forwarding<adapted_env_t_> Self, typename... NewQs>
            constexpr static auto
            readapt_impl(Self&& self, NewQs&&... qvs) noexcept(adapted_env_t_::is_nothrow_readaptable<Self>(
                typename readapted_env_t<std::remove_cvref_t<NewQs>...>::adapted_query_values{}, type_list<NewQs...>{}))
            {
                using result_t = readapted_env_t<std::remove_cvref_t<NewQs>...>;
                auto qvs_tup = std::tuple<NewQs&&...>(static_cast<NewQs&&>(qvs)...);
                const auto get_ref = [&]<typename Q>(type_tag_t<Q>) -> auto&&
                {
                    constexpr auto idx = meta::find_lv<type_list<std::remove_cvref_t<NewQs>...>, Q>;
                    if constexpr (idx == meta::npos) // Not in new queries
                    {
                        constexpr auto original_idx = meta::find_lv<type_list<Qs...>, Q>;
                        return std::get<original_idx>(static_cast<Self&&>(self).values_);
                    }
                    else
                    {
                        using qs_ref = typename meta::nth_type_q<idx>::template fn<NewQs...>;
                        return static_cast<qs_ref&&>(std::get<idx>(qvs_tup)).value;
                    }
                };
                return [&]<typename... AllQs>(type_list<AllQs...>) {
                    return result_t(static_cast<Self&&>(self).base_, get_ref(type_tag<AllQs>)...);
                }(typename result_t::adapted_query_values{});
            }

        public:
            // clang-format off
            template <typename... NewQs>
            constexpr auto adapt(NewQs&&... qvs) const& CLU_SINGLE_RETURN(
                adapted_env_t_::readapt_impl(*this, static_cast<NewQs&&>(qvs)...));

            template <typename... NewQs>
            constexpr auto adapt(NewQs&&... qvs) && CLU_SINGLE_RETURN(
                adapted_env_t_::readapt_impl(std::move(*this), static_cast<NewQs&&>(qvs)...));
            // clang-format on

            template <fwd_query Qry, typename... Ts>
                requires(!meta::contains_lv<adapted_queries, Qry>) && //
                tag_invocable<Qry, const Env&, Ts...> && fwd_query<Qry>
            constexpr decltype(auto) tag_invoke(Qry, Ts&&... args) //
                const noexcept(nothrow_tag_invocable<Qry, const Env&, Ts...>)
            {
                return clu::tag_invoke(Qry{}, base_, static_cast<Ts&&>(args)...);
            }

            template <typename Qry>
                requires meta::contains_lv<adapted_queries, Qry>
            constexpr auto tag_invoke(Qry) const noexcept
            {
                return std::get<meta::find_lv<adapted_queries, Qry>>(values_);
            }

        private:
            CLU_NO_UNIQUE_ADDRESS Env base_;
            CLU_NO_UNIQUE_ADDRESS std::tuple<typename Qs::type...> values_;
        };

        template <typename Env, typename... Qs>
        constexpr auto adapt_env_tag(std::false_type, Env&& base, Qs&&... qvs) CLU_SINGLE_RETURN( //
            adapted_env_t_<std::remove_cvref_t<Env>, Qs...>(static_cast<Env&&>(base), static_cast<Qs&&>(qvs).value...));

        template <typename Env, typename... Qs>
        constexpr auto adapt_env_tag(std::true_type, Env&& base, Qs&&... qvs) CLU_SINGLE_RETURN( //
            static_cast<Env&&>(base).adapt(static_cast<Qs&&>(qvs)...));

        template <typename Env, typename... Qs>
            requires(template_of<Qs, query_value> && ...)
        constexpr auto adapt_env(Env&& base, Qs&&... qvs) CLU_SINGLE_RETURN(
            detail::adapt_env_tag(std::bool_constant<template_of<std::remove_cvref_t<Env>, adapted_env_t_>>{},
                static_cast<Env&&>(base), static_cast<Qs&&>(qvs)...));

        template <typename Env, typename... Qs>
        using adapted_env_t = decltype(detail::adapt_env(std::declval<Env>(), std::declval<Qs>()...));
    } // namespace detail

    using detail::adapt_env;
    using detail::query_value;
    using detail::adapted_env_t;
} // namespace clu

// clang-format off
/**
 * \brief Specify a query CPO type as a forwarding query.
 * \param type The query type
 */
#define CLU_FORWARDING_QUERY(type)                                                                                            \
    constexpr bool tag_invoke(::clu::forwarding_query_t) const noexcept { return true; }                                      \
    static_assert(true)
// clang-format on
