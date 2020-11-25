#pragma once

#include "fmap.h"
#include "when_all_ready.h"

namespace clu
{
    namespace detail
    {
        template <typename... Ts, size_t... Is>
        auto extract_tuple_outcomes_impl(std::tuple<outcome<Ts>...>&& outcomes, std::index_sequence<Is...>)
        {
            return std::tuple<Ts...>{ *std::move(std::get<Is>(outcomes))... };
        }

        inline const auto extract_tuple_outcomes = []<typename... Ts>(std::tuple<outcome<Ts>...>&& outcomes)
        {
            return extract_tuple_outcomes_impl(std::move(outcomes), std::index_sequence_for<Ts...>{});
        };

        inline const auto extract_vector_outcomes = []<typename T>(std::vector<outcome<T>>&& outcomes)
        {
            std::vector<T> result;
            result.reserve(outcomes.size());
            for (auto&& elem : outcomes)
                result.push_back(*std::move(elem));
            return result;
        };
    }

    template <typename... Ts> requires (awaitable<std::unwrap_reference_t<Ts>> && ...)
    [[nodiscard]] auto when_all(Ts&&... awaitables)
    {
        return fmap(
            detail::when_all_ready_tuple_awaitable(std::forward<Ts>(awaitables)...),
            detail::extract_tuple_outcomes);
    }

    template <awaitable T>
    [[nodiscard]] auto when_all(std::vector<T> awaitables)
    {
        return fmap(
            detail::when_all_ready_vector_awaitable<T>(std::move(awaitables)),
            detail::extract_vector_outcomes);
    }
}
