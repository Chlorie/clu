#pragma once

#include <compare>

#include "concepts.h"

namespace clu
{
    namespace detail
    {
        template <template <typename> typename> struct check_type_alias_exists {};
    }

    // @formatter:off
    template <typename T>
    concept stoppable_token =
        std::copy_constructible<T> &&
        std::move_constructible<T> &&
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<T> &&
        std::equality_comparable<T> &&
        requires(const T& token)
        {
            { token.stop_requested() } noexcept -> boolean_testable;
            { token.stop_possible() } noexcept -> boolean_testable;
            typename detail::check_type_alias_exists<T::template callback_type>;
        };

    template <typename Token, typename Callback, typename Init = Callback>
    concept stoppable_token_for =
        stoppable_token<Token> &&
        std::invocable<Callback> &&
        requires { typename Token::template callback_type<Callback>; } &&
        std::constructible_from<Callback, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, Token, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, Token&, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, const Token, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, const Token&, Init>;

    template <typename T>
    concept unstoppable_token =
        stoppable_token<T> &&
        requires
        {
            { T::stop_possible() } -> boolean_testable;
        } &&
        (!T::stop_possible());
    // @formatter:on

    class never_stop_token
    {
    private:
        struct callback
        {
            explicit callback(never_stop_token, auto&&) noexcept {}
        };

    public:
        template <typename> using callback_type = callback;
        [[nodiscard]] static constexpr bool stop_possible() noexcept { return false; }
        [[nodiscard]] static constexpr bool stop_requested() noexcept { return false; }
        [[nodiscard]] friend constexpr bool operator==(never_stop_token, never_stop_token) noexcept = default;
    };

    // TODO: inplace_stop_token, inplace_stop_source, inplace_stop_callback
}
