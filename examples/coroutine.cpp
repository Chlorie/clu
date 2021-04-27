#include <iostream>
#include <semaphore>
#include <clu/experimental/coroutine/sync_wait.h>
#include <clu/experimental/coroutine/async_mutex.h>
#include <clu/experimental/coroutine/race.h>
#include <clu/experimental/coroutine/cancellable_task.h>

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

using steady_duration = std::chrono::steady_clock::duration;

struct wait_on_detached_thread
{
    steady_duration duration;
    bool cancelled = false;
    std::binary_semaphore semaphore{ 0 };

    bool await_ready() const noexcept { return false; }

    void await_suspend(const std::coroutine_handle<> handle)
    {
        std::thread([handle, this]
        {
            (void)semaphore.try_acquire_for(duration);
            handle.resume();
        }).detach();
    }

    bool await_resume() const noexcept { return cancelled; }

    void cancel()
    {
        if (!std::exchange(cancelled, true))
            semaphore.release();
    }
};

clu::async_mutex cout_mutex;

clu::cancellable_task<> trash_timer()
{
    size_t counter = 0;
    while (true)
    {
        const bool cancelled = co_await wait_on_detached_thread(1s);
        if (cancelled)
        {
            std::cout << "Cancelled\n";
            co_return;
        }
        auto _ = co_await cout_mutex.async_lock_scoped();
        std::cout << "Counting " << ++counter << "s\n";
    }
}

clu::cancellable_task<> waiter()
{
    {
        auto _ = co_await cout_mutex.async_lock_scoped();
        std::cout << "Wait for 3.14s\n";
    }
    co_await wait_on_detached_thread(3140ms);
}

int main() // NOLINT
{
    sync_wait(race(
        trash_timer(),
        waiter()
    ));
}
