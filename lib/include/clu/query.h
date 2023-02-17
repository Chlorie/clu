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

    namespace detail::qry
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
                    return tag_invoke(*this, static_cast<T&&>(value));
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
                    return tag_invoke(*this, Qry{}) ? true : false;
                }
                else
                    return std::derived_from<Qry, forwarding_query_t>;
            }
        };

        // clang-format off
        template <typename Qry>
        concept fwd_query = forwarding_query_t{}(Qry{});
        // clang-format on
    } // namespace detail::qry

    using detail::qry::forwarding_query_t;
    inline constexpr forwarding_query_t forwarding_query{};

    using detail::qry::get_env_t;
    inline constexpr get_env_t get_env{};

    // clang-format off
    template <typename T>
    concept environment_provider =
        requires(const std::remove_cvref_t<T>& prov) { { get_env(prov) } -> queryable; };
    // clang-format on

    template <typename T>
    using env_of_t = call_result_t<get_env_t, T>;

    namespace detail::env_adpt
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
        struct env_t_
        {
            class type;
        };

        template <typename Env, typename... Qs>
            requires(template_of<Qs, query_value> && ...)
        using adapted_env_t = typename env_t_<std::remove_cvref_t<Env>, Qs...>::type;

        template <typename Env, typename... Qs>
        class env_t_<Env, Qs...>::type
        {
        public:
            // clang-format off
            template <typename Env2, typename... Ts>
            constexpr explicit type(Env2&& base, Ts&&... values) noexcept(
                std::is_nothrow_constructible_v<Env, Env2> &&
                (std::is_nothrow_constructible_v<typename Qs::type, Ts> && ...)):
                base_(static_cast<Env2&&>(base)),
                values_(static_cast<Ts&&>(values)...) {}
            // clang-format on

        private:
            using adapted_queries = type_list<typename Qs::query_type...>;

            CLU_NO_UNIQUE_ADDRESS Env base_;
            CLU_NO_UNIQUE_ADDRESS std::tuple<typename Qs::type...> values_;

            template <qry::fwd_query Qry, typename... Ts>
                requires(!meta::contains_lv<adapted_queries, Qry>) && //
                tag_invocable<Qry, const Env&, Ts...> && qry::fwd_query<Qry>
            constexpr friend decltype(auto) tag_invoke(Qry, const type& self, Ts&&... args) //
                noexcept(nothrow_tag_invocable<Qry, const Env&, Ts...>)
            {
                return tag_invoke(Qry{}, self.base_, static_cast<Ts&&>(args)...);
            }

            template <typename Qry>
                requires meta::contains_lv<adapted_queries, Qry>
            constexpr friend auto tag_invoke(Qry, const type& self) noexcept
            {
                return std::get<meta::find_lv<adapted_queries, Qry>>(self.values_);
            }
        };

        template <typename Env, typename... Qs>
            requires(template_of<Qs, query_value> && ...)
        constexpr auto adapt_env(Env&& base, Qs&&... qvs) noexcept(
            std::is_nothrow_constructible_v<adapted_env_t<Env, Qs...>, //
                Env, typename std::remove_cvref_t<Qs>::type...>)
        {
            return adapted_env_t<Env, Qs...>( //
                static_cast<Env&&>(base), static_cast<Qs&&>(qvs).value...);
        }
    } // namespace detail::env_adpt

    using detail::env_adpt::adapt_env;
    using detail::env_adpt::query_value;
    using detail::env_adpt::adapted_env_t;
} // namespace clu

// clang-format off
/**
 * \brief Specify a query CPO type as a forwarding query.
 * \param type The query type
 */
#define CLU_FORWARDING_QUERY(type)                                                                                            \
    constexpr friend bool tag_invoke(::clu::forwarding_query_t, type) noexcept { return true; }                               \
    static_assert(true)
// clang-format on
