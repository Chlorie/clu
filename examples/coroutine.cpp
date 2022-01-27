#include <iostream>
#include <string>
#include <chrono>
#include <mutex>

#include <clu/execution/execution_traits.h>
#include <clu/execution/sender_consumers.h>
#include <clu/execution/sender_factories.h>
#include <clu/execution/contexts.h>
#include <clu/execution/algorithms.h>

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
namespace ed = ex::detail;

template <typename T>
[[deprecated]] void print_type() {}

int main() // NOLINT
{
    using then_snd = ed::then_snd<ex::run_loop::snd_t, void(*)()>;
    using start_recv = ex::start_detached_t::recv_t<then_snd>;
    using then_recv = ed::then_recv<start_recv, void(*)()>;
    // print_type<ex::completion_signatures_of_t<then_snd>>();
    static_assert(clu::tag_invocable<ex::connect_t, then_snd, start_recv>);
    // print_thread_id();
    // ex::single_thread_context ctx;
    // auto schd = ctx.get_scheduler();
    // ex::start_detached(ex::schedule(schd) | ex::then(print_thread_id));
    // ctx.finish();
}
