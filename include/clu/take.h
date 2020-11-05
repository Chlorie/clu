#pragma once

namespace clu
{
    template <typename T>
    T take(T& value)
    {
        T old = static_cast<T&&>(value);
        value = T{};
        return old;
    }
}
