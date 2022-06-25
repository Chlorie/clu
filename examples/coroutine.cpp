#include <iostream>
#include <chrono>
#include <mutex>
#include <format>
#include <memory_resource>

#include <clu/execution_contexts.h>
#include <clu/async.h>
#include <clu/async/channel.h>
#include <clu/chrono_utils.h>
#include <clu/task.h>
#include <clu/flow.h>
#include <clu/indices.h>
#include <clu/scope.h>
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

template <std::ranges::sized_range R>
std::string to_string(R&& range)
{
    namespace sr = std::ranges;
    if (sr::empty(range))
        return "[]";
    auto iter = sr::begin(range);
    const auto sent = sr::end(range);
    std::string result = std::format("[{}", *iter++);
    for (; iter != sent; ++iter)
        result += std::format(", {}", *iter);
    result += ']';
    return result;
}

template <typename... Ts>
void log(const std::string_view fmt, Ts&&... args)
{
    std::cout << std::format("[{}] {}\n", //
        clu::local_now(), std::vformat(fmt, std::make_format_args(args...)));
}

bool is_prime(const int value)
{
    if (value < 2)
        return false;
    for (int i = 2; i * i <= value; i++)
        if (value % i == 0)
            return false;
    return true;
}

clu::flow<int> tick(const int max)
{
    for (int i = 1; i <= max; i++)
    {
        log("[->] {}", i);
        co_yield i;
        co_await (timer() | ex::schedule_after(0.2s));
    }
}

clu::task<void> task()
{
    const std::vector<int> vec = co_await ( //
        tick(15) //
        | ex::filter(is_prime | clu::compose(ex::just)) //
        | ex::upon_each(
              [](const int value)
              {
                  log("[<-] {}", value);
                  return value;
              }) //
        | ex::into_vector() //
    );
    log("Collected vector: {}", to_string(vec));
}

class recording_memory_resource final : public std::pmr::memory_resource
{
public:
    recording_memory_resource() = default;
    std::size_t allocate_bytes() const noexcept { return allocated_; }

private:
    std::size_t allocated_ = 0;

    void* do_allocate(const size_t bytes, const size_t align) override
    {
        clu::scope_success guard{[&]
            {
                log("Allocated {} bytes", bytes);
                allocated_ += align;
            }};
        return std::pmr::get_default_resource()->allocate(bytes, align);
    }

    void do_deallocate(void* ptr, const size_t bytes, const size_t align) override
    {
        log("Deallocated {} bytes", bytes);
        allocated_ -= bytes;
        std::pmr::get_default_resource()->deallocate(ptr, bytes, align);
    }

    bool do_is_equal(const memory_resource&) const noexcept override { return false; }
};

clu::task<void> test()
{
    recording_memory_resource mem;
    clu::async::scope scope;
    (void)scope.spawn_future(ex::just_from([] { log("Test"); }), //
        std::pmr::polymorphic_allocator(&mem));
    co_await scope.deplete_async();
}

int main() // NOLINT
{
    // clang-format off
    try { clu::this_thread::sync_wait(test()); }
    catch (const std::exception& exc) { std::cout << exc.what() << '\n'; }
    // clang-format on
}
