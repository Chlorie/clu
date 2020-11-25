#include <iostream>
#include <numeric>
#include <clu/coroutine/task.h>
#include <clu/coroutine/sync_wait.h>
#include <clu/coroutine/when_all.h>

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

struct to_detached_thread
{
    bool await_ready() const noexcept { return false; }

    void await_suspend(const std::coroutine_handle<> handle)
    {
        std::thread([handle]()
        {
            handle.resume();
        }).detach();
    }

    void await_resume() const noexcept {}
};

clu::task<int> get_answer()
{
    co_await to_detached_thread();
    std::this_thread::sleep_for(1s);
    co_return 42;
}

clu::task<std::pair<int, int>> sequenced()
{
    const int first = co_await get_answer();
    const int second = co_await get_answer();
    co_return std::pair{ first, second };
}

clu::task<std::pair<int, int>> concurrent()
{
    const auto [first, second] = co_await when_all(get_answer(), get_answer());
    co_return std::pair(first, second);
}

clu::task<std::vector<int>> dynamic(const size_t size)
{
    std::vector<clu::task<int>> tasks;
    tasks.reserve(size);
    for (size_t i = 0; i < size; i++) tasks.push_back(get_answer());
    co_return co_await when_all(std::move(tasks));
}

int main() // NOLINT
{
    time_call([]
    {
        const auto [first, second] = sync_wait(sequenced());
        std::cout << "The answers are " << first << " and " << second << '\n';
    });
    time_call([]
    {
        const auto [first, second] = sync_wait(concurrent());
        std::cout << "The answers are " << first << " and " << second << '\n';
    });
    time_call([]
    {
        const auto result = sync_wait(dynamic(100));
        const auto sum = std::accumulate(result.begin(), result.end(), 0);
        std::cout << "The sum is " << sum << '\n';
    });
}
