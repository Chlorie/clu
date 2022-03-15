#include <iostream>
#include <string>
#include <chrono>
#include <mutex>
#include <array>

#include <clu/execution.h>
#include <clu/task.h>
#include <clu/generator.h>

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

clu::task<int> f() { co_return 42; }

clu::task<void> g()
{
    for (std::size_t i = 0; i < 5; i++)
    {
        co_await wait_on_detached_thread(0.2s);
        const int answer = co_await f();
        std::cout << "The answer is " << answer << '\n';
    }
}

auto maybe_throw(const bool do_throw)
{
    return [do_throw]
    {
        if (do_throw)
            throw std::runtime_error("wat");
    };
}

int happy_path() { return 69; }
double sad_path(const std::exception_ptr&) { return 420.; }

int main() // NOLINT
{
    clu::this_thread::sync_wait(g() | ex::then(print_thread_id));
    for (const bool do_throw : {false, true})
    {
        // clang-format off
        print_thread_id(); // Main thread id
        const auto res = clu::this_thread::sync_wait_with_variant(
            to_detached_thread()
            | ex::then(print_thread_id) // New detached thread id
            | ex::then(maybe_throw(do_throw)) 
            | ex::then(happy_path) // -> 69 if didn't throw
            | ex::upon_error(sad_path) // -> 420 if did throw
        );
        // clang-format on
        if (res)
            std::visit(
                [](const auto& tup)
                {
                    const auto [v] = tup;
                    std::cout << "result is " << v << '\n';
                },
                std::get<0>(*res));
        else
            std::cout << "cancelled lol\n";
    }
}
