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

#include <conio.h>

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
    static clu::timer_thread_context ctx;
    return ctx.get_scheduler();
}

auto get_pool()
{
    static clu::static_thread_pool pool{8};
    return pool.get_scheduler();
}

clu::async::shared_mutex mut;

clu::task<void> shared(const int id)
{
    std::cout << std::format("[SHARED {}] Waiting for the lock\n", id);
    {
        auto _ = co_await lock_shared_scoped_async(mut);
        std::cout << std::format("[SHARED {}] Got the lock\n", id);
        co_await ex::schedule_after(get_timer(), 3s);
    }
    std::cout << std::format("[SHARED {}] Released the lock\n", id);
}

clu::task<void> unique(const int id)
{
    std::cout << std::format("[UNIQUE {}] Waiting for the lock\n", id);
    {
        auto _ = co_await lock_scoped_async(mut);
        std::cout << std::format("[UNIQUE {}] Got the lock\n", id);
        co_await ex::schedule_after(get_timer(), 3s);
    }
    std::cout << std::format("[UNIQUE {}] Released the lock\n", id);
}

clu::task<void> task()
{
    clu::async::scope scope;
    int id = 0;
    while (true)
    {
        int c = _getch();
        if (c == 's')
            scope.spawn(ex::on(get_pool(), shared(id++)));
        else if (c == 'u')
            scope.spawn(ex::on(get_pool(), unique(id++)));
        else if (c == 'q')
            break;
    }
    co_await scope.deplete_async();
}

int main() // NOLINT
{
    clu::this_thread::sync_wait(task());
}
