#include <iostream>
#include <string>
#include <chrono>
#include <mutex>

#include <clu/execution.h>

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

// clu::async_mutex cout_mutex;
// 
// clu::cancellable_task<> trash_timer()
// {
//     size_t counter = 0;
//     while (true)
//     {
//         const bool cancelled = co_await wait_on_detached_thread(1s);
//         if (cancelled)
//         {
//             std::cout << "Cancelled\n";
//             co_return;
//         }
//         auto _ = co_await cout_mutex.async_lock_scoped();
//         std::cout << "Counting " << ++counter << "s\n";
//     }
// }

namespace ex = clu::exec;

struct to_detached_thread
{
    bool await_ready() const noexcept { return false; }
    void await_suspend(const clu::coro::coroutine_handle<> handle) { std::thread(handle).detach(); }
    void await_resume() const noexcept {}
};

auto maybe_throw(const bool do_throw)
{
    return [do_throw] { if (do_throw) throw std::runtime_error("wat"); };
}

int happy_path() { return 69; }
double sad_path(const std::exception_ptr&) { return 420.; }

int main() // NOLINT
{
    for (const bool do_throw : { false, true })
    {
        print_thread_id(); // Main thread id
        const auto res =
            clu::this_thread::sync_wait_with_variant(
                to_detached_thread()
                | ex::then(print_thread_id) // New detached thread id
                | ex::then(maybe_throw(do_throw))
                | ex::then(happy_path) // -> 69 if didn't throw
                | ex::upon_error(sad_path) // -> 420 if did throw
            );
        if (res)
            std::visit([](const auto& tup)
            {
                const auto [v] = tup;
                std::cout << "result is " << v << '\n';
            }, std::get<0>(*res));
        else
            std::cout << "cancelled lol\n";
    }
}
