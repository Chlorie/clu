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
#include <clu/functional.h>

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

auto timer()
{
    static clu::timer_thread_context ctx;
    return ctx.get_scheduler();
}

auto thread_pool()
{
    static clu::static_thread_pool pool{8};
    return pool.get_scheduler();
}

template <typename... Ts>
void log(const std::string_view fmt, Ts&&... args)
{
    std::cout << std::format("[{}] {}\n", //
        clu::local_now(), std::vformat(fmt, std::make_format_args(args...)));
}

clu::flow<int> tick(const int max)
{
    for (int i = 1; i <= max; i++)
    {
        log("Yielding {}...", i);
        co_yield i;
        co_await (timer() | ex::schedule_after(0.25s));
    }
}

clu::task<void> task()
{
    const auto cleanup_log = ex::just_from([] { log("Executing clean up"); });
    auto sum_task = tick(15) //
        | ex::adapt_cleanup([&](auto&&) { return cleanup_log; }) //
        | ex::reduce(1_uz, std::multiplies{} | clu::compose(ex::just));
    auto timeout = timer() | ex::schedule_after(12s);
    const auto sum = co_await ex::stop_when(std::move(sum_task), std::move(timeout));
    log("15! = {}", sum);
}

int main() // NOLINT
{
    // clang-format off
    try { clu::this_thread::sync_wait(task()); }
    catch (const std::exception& exc) { std::cout << exc.what() << '\n'; }
    // clang-format on
}
