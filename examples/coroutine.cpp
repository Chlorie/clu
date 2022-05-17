#include <iostream>
#include <chrono>
#include <mutex>
#include <format>

#include <clu/execution_contexts.h>
#include <clu/async.h>
#include <clu/chrono_utils.h>
#include <clu/task.h>
#include <clu/indices.h>
#include <clu/random.h>

using namespace std::literals;
using namespace clu::literals;
namespace chr = std::chrono;

template <typename T>
struct print;

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

std::string this_thread_id()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return std::move(ss).str();
}

auto get_timer()
{
    struct raii_guard
    {
        clu::timer_thread_context ctx;
        ~raii_guard() noexcept { ctx.finish(); }
    };
    static raii_guard guard;
    return guard.ctx.get_scheduler();
}

clu::task<void> counter()
{
    for (int i = 0;; i++)
    {
        std::cout << std::format("Counting: {}\n", i);
        co_await ex::schedule_after(get_timer(), 1s);
    }
}

std::atomic_int total = 0;
clu::task<void> wait(const std::size_t ms)
{
    co_await ex::schedule_after(get_timer(), chr::milliseconds(ms));
    total.fetch_add(1, std::memory_order::relaxed);
}

clu::task<void> test()
{
    clu::async_scope scope;
    std::vector<clu::task<void>> waits;
    time_call(
        [&]
        {
            for (auto _ : clu::indices(1'000))
                waits.emplace_back(scope.spawn_future(wait(clu::randint(100, 10000))) | clu::as_task());
        });
    co_await wait(5000);
    scope.request_stop();
    co_await scope.deplete_async();
    std::cout << "Total waited: " << total.load(std::memory_order::relaxed) << '\n';
}

clu::task<void> task() { co_await (counter() | ex::stop_when(wait(3142))); }

int main() // NOLINT
{
    clu::this_thread::sync_wait(test());
}
