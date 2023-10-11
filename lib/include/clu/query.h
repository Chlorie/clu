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
            (T&& value) noexcept
            {
                if constexpr (tag_invocable<get_env_t, T>)
                {
                    static_assert(queryable<tag_invoke_result_t<get_env_t, T>>, "get_env should return a queryable");
                    return tag_invoke(get_env_t{}, static_cast<T&&>(value));
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
        // TODO: optimize the case of multiple adaptions onto the same environment

        template <typename Qry, typename T>
        struct query_value
        {
            using query_type = Qry;
            using type = T;
            CLU_NO_UNIQUE_ADDRESS query_type query;
            CLU_NO_UNIQUE_ADDRESS type value;
        };

        template <typename Env, typename... Qs>
        class adapted_env_t
        {
        public:
            // clang-format off
            template <typename Env2, typename... Ts>
            constexpr explicit adapted_env_t(Env2&& base, Ts&&... values) noexcept(
                std::is_nothrow_constructible_v<Env, Env2> &&
                (std::is_nothrow_constructible_v<typename Qs::type, Ts> && ...)):
                base_(static_cast<Env2&&>(base)),
                values_(static_cast<Ts&&>(values)...) {}
            // clang-format on

            using adapted_queries = type_list<typename Qs::query_type...>;

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
            requires(template_of<Qs, query_value> && ...)
        constexpr auto adapt_env(Env&& base, Qs&&... qvs) noexcept(
            std::is_nothrow_constructible_v<adapted_env_t<std::remove_cvref_t<Env>, Qs...>, //
                Env, typename std::remove_cvref_t<Qs>::type...>)
        {
            return adapted_env_t<std::remove_cvref_t<Env>, Qs...>( //
                static_cast<Env&&>(base), static_cast<Qs&&>(qvs).value...);
        }
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
