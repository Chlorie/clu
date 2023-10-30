#pragma once

#include <optional>

#include "basic.h"
#include "scheduling.h"
#include "../../piper.h"

namespace clu::exec
{
    struct transfer_t
    {
        template <sender Snd, scheduler Schd>
        CLU_STATIC_CALL_OPERATOR(auto)
        (Snd&& snd, Schd&& schd)
        {
            if constexpr (detail::customized_sender_algorithm<transfer_t, set_value_t, Snd, Schd>)
                return detail::invoke_customized_sender_algorithm<transfer_t, set_value_t>(
                    static_cast<Snd&&>(snd), static_cast<Schd&&>(schd));
            else
                return schedule_from(static_cast<Schd&&>(schd), static_cast<Snd&&>(snd));
        }

        template <scheduler Schd>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(Schd&& schd) noexcept
        {
            return clu::make_piper(clu::bind_back(transfer_t{}, static_cast<Schd&&>(schd)));
        }
    };
    inline constexpr transfer_t transfer{};

    struct transfer_just_t
    {
        template <scheduler Schd, typename... Ts>
        CLU_STATIC_CALL_OPERATOR(auto)
        (Schd&& schd, Ts&&... args)
        {
            if constexpr (tag_invocable<transfer_just_t, Schd, Ts...>)
            {
                static_assert(sender<tag_invoke_result_t<transfer_just_t, Schd, Ts...>>,
                    "customization of transfer_just should return a sender");
                return clu::tag_invoke(transfer_just_t{}, static_cast<Schd&&>(schd), static_cast<Ts&&>(args)...);
            }
            else
                return transfer(exec::just(static_cast<Ts&&>(args)...), static_cast<Schd&&>(schd));
        }
    };
    inline constexpr transfer_just_t transfer_just{};

    struct just_from_t
    {
        template <std::invocable F>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(F&& func)
        {
            return just() | then(static_cast<F&&>(func));
        }
    };
    inline constexpr just_from_t just_from{};

    struct defer_t
    {
        template <std::invocable F>
            requires sender<std::invoke_result_t<F>>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(F&& func)
        {
            return just() | let_value(static_cast<F&&>(func));
        }
    };
    inline constexpr defer_t defer{};

    struct stopped_as_optional_t
    {
        template <sender S>
        CLU_STATIC_CALL_OPERATOR(auto)
        (S&& snd)
        {
            return read(std::identity{}) //
                | let_value(let_value_cb<std::decay_t<S>>{static_cast<S&&>(snd)});
        }

        constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(stopped_as_optional_t{}); }

    private:
        template <typename S>
        struct let_value_cb
        {
            S snd;

            template <typename E>
                requires detail::single_sender<S, E>
            auto operator()(const E&)
            {
                using value_t = detail::single_sender_value_type<S, E>;
                using optional_t = std::optional<value_t>;
                return static_cast<S&&>(snd) //
                    | then([]<typename T>(T&& t) { return optional_t{static_cast<T&&>(t)}; }) //
                    | let_stopped([]() noexcept { return just(optional_t{}); });
            }
        };
    };
    inline constexpr stopped_as_optional_t stopped_as_optional{};

    struct stopped_as_error_t
    {
        template <sender S, movable_value E>
        CLU_STATIC_CALL_OPERATOR(auto)
        (S&& snd, E&& err)
        {
            return let_stopped(static_cast<S&&>(snd),
                [err = static_cast<E&&>(err)]() mutable { return just_error(static_cast<E&&>(err)); });
        }

        template <movable_value E>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(E&& err)
        {
            return clu::make_piper(clu::bind_back(stopped_as_error_t{}, static_cast<E&&>(err)));
        }
    };
    inline constexpr stopped_as_error_t stopped_as_error{};

    struct with_stop_token_t
    {
        template <sender S>
        CLU_STATIC_CALL_OPERATOR(auto)
        (S&& snd, const stoppable_token auto token)
        {
            return with_query_value(static_cast<S&&>(snd), get_stop_token, token);
        }
    };
    inline constexpr with_stop_token_t with_stop_token{};

    struct finally_t
    {
        template <sender S, sender C>
        CLU_STATIC_CALL_OPERATOR(auto)
        (S&& src, C&& cleanup)
        {
            return static_cast<S&&>(src) //
                | materialize() //
                | let_value(let_value_cb<std::decay_t<C>>{static_cast<C&&>(cleanup)});
        }

        template <sender C>
        CLU_STATIC_CALL_OPERATOR(auto)
        (C&& cleanup)
        {
            return clu::make_piper(clu::bind_back(finally_t{}, static_cast<C&&>(cleanup)));
        }

    private:
        template <typename C>
        struct let_value_cb
        {
            C cleanup;

            template <typename Cpo, typename... Ts>
            auto operator()(Cpo, Ts&&... values)
            {
                return with_stop_token(std::move(cleanup), never_stop_token{}) //
                    | materialize() //
                    | let_value(
                          [... vs = static_cast<Ts&&>(values)]<typename Cpo2, typename... Us>(Cpo2, Us&&... err) mutable
                          {
                              if constexpr (std::is_same_v<Cpo2, set_value_t>) // Clean up succeeded
                                  return detail::just::snd_t<Cpo, Ts...>(std::move(vs)...);
                              else // Clean up finished with error/stopped
                                  return detail::just::snd_t<Cpo2, Us...>(static_cast<Us&&>(err)...);
                          });
            }
        };
    };
    inline constexpr finally_t finally{};

    struct into_tuple_t
    {
        template <sender S>
        CLU_STATIC_CALL_OPERATOR(auto)
        (S&& snd)
        {
            // clang-format off
            return then(
                static_cast<S&&>(snd),
                []<typename... Ts>(Ts&&... args) noexcept(
                    (std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts> && ...))
                { return detail::decayed_tuple<Ts...>(static_cast<Ts&&>(args)...); }
            );
            // clang-format on
        }

        constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(into_tuple_t{}); }
    };
    inline constexpr into_tuple_t into_tuple{};
} // namespace clu::exec
