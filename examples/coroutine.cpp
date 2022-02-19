#include <iostream>
#include <string>
#include <chrono>
#include <mutex>

#include <clu/execution.h>
#include <clu/concurrency.h>

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

struct to_detached_thread
{
    bool await_ready() const noexcept { return false; }

    void await_suspend(const clu::coro::coroutine_handle<> handle)
    {
        std::thread([handle]
        {
            std::cout << "lolz\n";
            auto h = handle;
            h.resume();
        }).detach();
    }

    void await_resume() const noexcept {}
};

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

auto maybe_throw(const bool do_throw)
{
    return [do_throw]
    {
        if (do_throw)
            throw std::runtime_error("wat");
    };
}

void happy_path()
{
    std::cout << "Yeah!\n";
}

void sad_path(const std::exception_ptr& eptr)
{
    try
    {
        std::rethrow_exception(eptr);
    }
    catch (const std::exception& e)
    {
        std::cout << "Sad things happened: " << e.what() << '\n';
    }
}

int main() // NOLINT
{
    print_thread_id();
    ex::single_thread_context ctx;
    auto schd = ctx.get_scheduler();
    ex::start_detached(
        ex::schedule(schd)
        | ex::then(print_thread_id)
        | ex::then(maybe_throw(true))
        | ex::then(happy_path)
        | ex::upon_error(sad_path));
    ctx.finish();
}
