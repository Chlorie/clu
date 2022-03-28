#include <iostream>
#include <string>
#include <chrono>
#include <mutex>
#include <array>

#include <clu/execution.h>
#include <clu/task.h>

using namespace std::literals;

template <typename F>
void time_call(F&& func)
{
    using clock = std::chrono::steady_clock;
    const auto begin = clock::now();
    std::forward<F>(func)();
    const auto end = clock::now();
    std::cout << "Function call completed in " << (end - begin) / 1ms << "ms\n";
}

std::mutex cout_mutex;

void print_thread_id()
{
    std::scoped_lock lock(cout_mutex);
    std::cout << "Thread ID: " << std::this_thread::get_id() << '\n';
}

namespace ex = clu::exec;

struct to_detached_thread
{
    bool await_ready() const noexcept { return false; }
    void await_suspend(const clu::coro::coroutine_handle<> handle) { std::thread(handle).detach(); }
    void await_resume() const noexcept {}
};

namespace wtf
{
    namespace get_pms
    {
        struct awaitable
        {
            template <typename P>
            friend auto tag_invoke(ex::as_awaitable_t, awaitable, P& promise)
            {
                struct awaiter
                {
                    P& pms;
                    bool await_ready() const noexcept { return true; }
                    void await_suspend(clu::coro::coroutine_handle<>) const noexcept { clu::unreachable(); }
                    P& await_resume() const noexcept { return pms; }
                };
                return awaiter{promise};
            }
        };
    } // namespace get_pms

    inline struct get_promise_t
    {
        constexpr auto operator()() const noexcept { return get_pms::awaitable{}; }
    } constexpr get_promise{};
} // namespace wtf

template <typename Dur>
struct wait_on_detached_thread
{
    Dur duration;

    bool await_ready() const noexcept { return false; }
    void await_suspend(clu::coro::coroutine_handle<> handle) const noexcept
    {
        std::thread(
            [=]() mutable
            {
                std::this_thread::sleep_for(duration);
                handle.resume();
            })
            .detach();
    }
    void await_resume() const noexcept {}
};

template <typename Dur>
wait_on_detached_thread(Dur) -> wait_on_detached_thread<Dur>;

int main() // NOLINT
{
    ex::single_thread_context ctx;
    // clang-format off
    clu::this_thread::sync_wait(
        ex::just_from(print_thread_id)      // prints main thread id
        | ex::transfer(ctx.get_scheduler()) // transfers to the single thread context
        | ex::then(print_thread_id)         // prints thread id of the context
    );
    // clang-format on
}
