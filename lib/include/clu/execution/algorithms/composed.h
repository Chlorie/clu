#pragma once

#include "basic.h"
#include "scheduling.h"
#include "../../piper.h"

namespace clu::exec
{
    struct transfer_t
    {
        template <sender Snd, scheduler Schd>
        auto operator()(Snd&& snd, Schd&& schd) const
        {
            if constexpr (detail::customized_sender_algorithm<transfer_t, set_value_t, Snd, Schd>)
                return detail::invoke_customized_sender_algorithm<transfer_t, set_value_t>(
                    static_cast<Snd&&>(snd), static_cast<Schd&&>(schd));
            else
                return schedule_from(static_cast<Schd&&>(schd), static_cast<Snd&&>(snd));
        }

        template <scheduler Schd>
        constexpr auto operator()(Schd&& schd) const noexcept
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Schd&&>(schd)));
        }
    };
    inline constexpr transfer_t transfer{};

    struct transfer_just_t
    {
        template <scheduler Schd, typename... Ts>
        auto operator()(Schd&& schd, Ts&&... args) const
        {
            if constexpr (tag_invocable<transfer_just_t, Schd, Ts...>)
            {
                static_assert(sender<tag_invoke_result_t<transfer_just_t, Schd, Ts...>>,
                    "customization of transfer_just should return a sender");
                return clu::tag_invoke(*this, static_cast<Schd&&>(schd), static_cast<Ts&&>(args)...);
            }
            else
                return transfer(exec::just(static_cast<Ts&&>(args)...), static_cast<Schd&&>(schd));
        }
    };
    inline constexpr transfer_just_t transfer_just{};

    struct just_from_t
    {
        template <std::invocable F>
        constexpr auto operator()(F&& func) const
        {
            return just() | then(static_cast<F&&>(func));
        }
    };
    inline constexpr just_from_t just_from{};

    struct defer_t
    {
        template <std::invocable F>
            requires sender<std::invoke_result_t<F>>
        constexpr auto operator()(F&& func) const
        { //
            return just() | let_value(static_cast<F&&>(func));
        }
    };
    inline constexpr defer_t defer{};

    struct stopped_as_optional_t
    {
        template <sender S>
        auto operator()(S&& snd) const
        {
            using detail::coro_utils::single_sender;
            using detail::coro_utils::single_sender_value_type;
            return read(std::identity{}) |
                let_value(
                    // clang-format off
                    [snd = static_cast<S&&>(snd)]<class E>
                        requires single_sender<S, E>
                        (const E&) mutable
                    {
                        using opt = std::optional<std::decay_t<single_sender_value_type<S, E>>>;
                        return static_cast<S&&>(snd)
                            | then([]<class T>(T&& t) { return opt{static_cast<T&&>(t)}; })
                            | let_stopped([]() noexcept { return just(opt{}); });
                    }
                    // clang-format on
                );
        }

        constexpr auto operator()() const noexcept { return make_piper(*this); }
    };
    inline constexpr stopped_as_optional_t stopped_as_optional{};

    struct stopped_as_error_t
    {
        template <sender S, movable_value E>
        auto operator()(S&& snd, E&& err) const
        {
            return let_stopped(static_cast<S&&>(snd),
                [err = static_cast<E&&>(err)]() mutable { return just_error(static_cast<E&&>(err)); });
        }

        template <movable_value E>
        constexpr auto operator()(E&& err) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<E&&>(err)));
        }
    };
    inline constexpr stopped_as_error_t stopped_as_error{};

    struct with_stop_token_t
    {
        template <sender S>
        auto operator()(S&& snd, const stoppable_token auto token) const
        {
            return with_query_value(static_cast<S&&>(snd), get_stop_token, token);
        }

        auto operator()(const stoppable_token auto token) const
        {
            return clu::make_piper(clu::bind_back(*this, token));
        }
    };
    inline constexpr with_stop_token_t with_stop_token{};

    struct finally_t
    {
        template <sender S, sender C>
        auto operator()(S&& src, C&& cleanup) const
        {
            return static_cast<S&&>(src) //
                | materialize() //
                | let_value(
                      [c = static_cast<C&&>(cleanup)]<typename Cpo, typename... Ts>(Cpo, Ts&&... values) mutable
                      {
                          return with_stop_token(std::move(c), never_stop_token{}) //
                              | materialize() //
                              | let_value(
                                    [... vs = static_cast<Ts&&>(values)]<typename Cpo2, typename... Us>(
                                        Cpo2, Us&&... err) mutable
                                    {
                                        if constexpr (std::is_same_v<Cpo2, set_value_t>) // Clean up succeeded
                                            return detail::just::snd_t<Cpo, Ts...>(std::move(vs)...);
                                        else // Clean up finished with error/stopped
                                            return detail::just::snd_t<Cpo2, Us...>(static_cast<Us&&>(err)...);
                                    });
                      });
        }

        template <sender C>
        auto operator()(C&& cleanup) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<C&&>(cleanup)));
        }
    };
    inline constexpr finally_t finally{};

    struct into_tuple_t
    {
        template <sender S>
        auto operator()(S&& snd) const
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

        constexpr auto operator()() const noexcept { return make_piper(*this); }
    };
    inline constexpr into_tuple_t into_tuple{};
} // namespace clu::exec