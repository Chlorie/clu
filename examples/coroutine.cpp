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

namespace clutest
{
    using clock = std::chrono::system_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    namespace detail::wait_detached
    {
        template <typename R>
        class ops_t
        {
        public:
            // clang-format off
            template <typename R2>
            ops_t(R2&& recv, const time_point tp):
                recv_(static_cast<R2&&>(recv)), tp_(tp) {}
            // clang-format on

        private:
            struct stop_callback
            {
                ops_t& self;
                void operator()() const noexcept { self.cv_.notify_one(); }
            };

            using stop_token_t = ex::stop_token_of_t<ex::env_of_t<R>>;
            using callback_t = typename stop_token_t::template callback_type<stop_callback>;

            CLU_NO_UNIQUE_ADDRESS R recv_;
            time_point tp_;
            std::mutex mut_;
            std::condition_variable cv_;
            std::optional<callback_t> callback_;

            void work(const stop_token_t& token) noexcept
            {
                {
                    std::unique_lock lck(mut_);
                    cv_.wait_until(lck, tp_, [=] { return token.stop_requested(); });
                }
                callback_.reset();
                if (token.stop_requested())
                    ex::set_stopped(static_cast<R&&>(recv_));
                else
                    ex::set_value(static_cast<R&&>(recv_));
            }

            friend void tag_invoke(ex::start_t, ops_t& self) noexcept
            {
                const auto token = ex::get_stop_token(ex::get_env(self.recv_));
                self.callback_.emplace(token, stop_callback{self});
                if (token.stop_requested())
                    ex::set_stopped(static_cast<R&&>(self.recv_));
                std::thread([&self, token] { self.work(token); }).detach();
            }
        };

        class snd_t
        {
        public:
            using completion_signatures = ex::completion_signatures< //
                ex::set_value_t(), ex::set_stopped_t()>;

            explicit snd_t(const time_point tp) noexcept: tp_(tp) {}

        private:
            time_point tp_;

            template <typename R>
            friend ops_t<std::remove_cvref_t<R>> tag_invoke(ex::connect_t, const snd_t type, R&& recv)
            {
                return {static_cast<R&&>(recv), type.tp_};
            }
        };
    } // namespace detail::wait_detached

    auto wait_on_detached_thread(const time_point tp)
    {
        using detail::wait_detached::snd_t;
        return snd_t(tp);
    }

    auto wait_on_detached_thread(const duration dur)
    {
        using detail::wait_detached::snd_t;
        return snd_t(clock::now() + dur);
    }
} // namespace clutest

std::string this_thread_id()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return std::move(ss).str();
}

clu::task<int> thing()
{
    co_await clutest::wait_on_detached_thread(500ms);
    co_return 42;
}

auto cleanup()
{
    return ex::just_from([] { std::cout << "Clean up\n"; });
}

struct get_promise_t
{
    template <typename P>
    friend auto tag_invoke(ex::as_awaitable_t, get_promise_t, P& promise) noexcept
    {
        struct awaiter
        {
            P* pms;
            bool await_ready() const noexcept { return true; }
            void await_suspend(clu::coro::coroutine_handle<>) const noexcept {}
            P* await_resume() const noexcept { return pms; }
        };
        return awaiter{&promise};
    }
} get_promise{};

clu::task<void> task()
{
    try
    {
        co_await ( //
            ex::just_error(std::exception("Weird error")) //
            | ex::finally(cleanup()) //
        );
    }
    catch (const std::exception& ex)
    {
        std::cout << std::format("Exception: {}\n", ex.what());
    }
    // co_await ex::with_query_value(ex::just(), ex::get_stop_token, clu::never_stop_token{});
}

int main() // NOLINT
{
    clu::this_thread::sync_wait(task());
}
