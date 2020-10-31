#pragma once

#include <chrono>

#include "concepts.h"

namespace clu
{
    namespace detail
    {
        template <typename T> struct is_duration : std::false_type {};
        template <typename Rep, typename Per> struct is_duration<std::chrono::duration<Rep, Per>> : std::true_type {};
    }

    template <typename T> concept duration = detail::is_duration<T>::value;

    template <typename T>
    concept scheduler = requires(T& sch)
    {
        { sch.schedule() } -> awaitable_of<void>;
    };

    template <scheduler S> auto schedule_on(S& sch) { return sch.schedule(); }
}
