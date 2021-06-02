#pragma once

namespace clu
{
    /// Reset input value to default and return the original value.
    ///
    /// Approximately equivalent to `std::exchange(value, {})`.
    template <typename T>
    constexpr T take(T& value)
    {
        T old = static_cast<T&&>(value);
        value = T{};
        return old;
    }
}
