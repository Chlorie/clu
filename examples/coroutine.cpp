#include <iostream>
#include <chrono>
#include <mutex>
#include <format>

#include <clu/execution_contexts.h>
#include <clu/async.h>
#include <clu/chrono_utils.h>
#include <clu/task.h>
#include <clu/indices.h>

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

clu::task<void> counter()
{
    for (int i = 0;; i++)
    {
        std::cout << std::format("Counting: {}\n", i);
        co_await ex::schedule_after(clu::new_thread_scheduler{}, 1s);
    }
}

clu::task<void> wait(const std::size_t ms)
{
    co_await ex::schedule_after(clu::new_thread_scheduler{}, chr::milliseconds(ms));
    std::cout << std::format("Waited {}ms\n", ms);
}

clu::task<void> task() { co_await (counter() | ex::stop_when(wait(3142))); }

int main() // NOLINT
{
    clu::this_thread::sync_wait(task());
}
