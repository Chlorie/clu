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

auto get_pool()
{
    struct raii_guard
    {
        clu::static_thread_pool pool{8};
        ~raii_guard() noexcept { pool.finish(); }
    };
    static raii_guard guard;
    return guard.pool.get_scheduler();
}

clu::async_mutex mut;
int value = 0;

clu::task<void> adder()
{
    co_await ex::schedule_after(get_timer(), chr::milliseconds(clu::randint(50, 100)));
    co_await mut.lock_async();
    clu::scope_exit guard{[] { mut.unlock(); }};
    value++;
}

clu::task<void> task()
{
    co_await ex::schedule(get_pool());
    clu::async_scope scope;
    for (auto _ : clu::indices(10000))
        scope.spawn(adder());
    co_await scope.deplete_async();
    std::cout << std::format("value = {}\n", value);
}

int main() // NOLINT
{
    clu::this_thread::sync_wait(task());
}
