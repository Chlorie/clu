#pragma once

#include <concepts>

namespace clu
{
    template <typename T, typename U>
    concept similar_to = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

    template <typename Inv, typename Res, typename... Ts>
    concept invocable_of = std::invocable<Inv> && std::convertible_to<std::invoke_result_t<Inv, Ts...>, Res>;
}
