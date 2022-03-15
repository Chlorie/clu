#pragma once

#include <chrono>

#include "concepts.h"

namespace clu
{
    inline auto local_now() { return std::chrono::current_zone()->to_local(std::chrono::system_clock::now()); }

    template <typename Func, typename... Args>
        requires callable<Func, Args...>
    auto timeit(Func&& func, Args&&... args)
    {
        const auto start = std::chrono::high_resolution_clock::now();
        (void)static_cast<Func&&>(func)(static_cast<Args&&>(args)...);
        const auto end = std::chrono::high_resolution_clock::now();
        return end - start;
    }

    namespace chrono_constants
    {
        using std::chrono::January;
        using std::chrono::February;
        using std::chrono::March;
        using std::chrono::April;
        using std::chrono::May;
        using std::chrono::June;
        using std::chrono::July;
        using std::chrono::August;
        using std::chrono::September;
        using std::chrono::October;
        using std::chrono::November;
        using std::chrono::December;

        using std::chrono::Sunday;
        using std::chrono::Monday;
        using std::chrono::Tuesday;
        using std::chrono::Wednesday;
        using std::chrono::Thursday;
        using std::chrono::Friday;
        using std::chrono::Saturday;
    } // namespace chrono_constants
} // namespace clu
