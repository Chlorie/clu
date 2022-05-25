#include <iostream>
#include <chrono>
#include <mutex>
#include <format>

#include <clu/execution_contexts.h>
#include <clu/async.h>
#include <clu/chrono_utils.h>
#include <clu/task.h>
#include <clu/flow.h>
#include <clu/indices.h>
#include <clu/random.h>
#include <clu/execution/stream.h>

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

clu::flow<int> tick()
{
    for (int i = 0;; i++)
    {
        co_yield i;
        if (i >= 5)
            co_await ex::stop();
        co_await ex::schedule_after(get_timer(), 1s);
    }
}

clu::task<void> task()
{
    auto stream = tick();
    while (true)
    {
        const auto value = co_await ex::next(stream);
        std::cout << std::format("Got value: {}\n", value);
    }
}

int main() // NOLINT
{
    // clang-format off
    try { clu::this_thread::sync_wait(task()); }
    catch (const std::exception& exc) { std::cout << exc.what() << '\n'; }
    // clang-format on
}
