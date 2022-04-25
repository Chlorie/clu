#include <iostream>
#include <chrono>
#include <mutex>

#include <clu/execution.h>
#include <clu/execution_contexts.h>
#include <clu/async.h>
#include <clu/chrono_utils.h>
#include <clu/task.h>

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

namespace wtf
{
    namespace get_pms
    {
        struct awaitable
        {
            template <typename P>
            friend auto tag_invoke(ex::as_awaitable_t, awaitable, P& promise)
            {
                struct awaiter
                {
                    P& pms;
                    bool await_ready() const noexcept { return true; }
                    void await_suspend(clu::coro::coroutine_handle<>) const noexcept { clu::unreachable(); }
                    P& await_resume() const noexcept { return pms; }
                };
                return awaiter{promise};
            }
        };
    } // namespace get_pms

    inline struct get_promise_t
    {
        constexpr auto operator()() const noexcept { return get_pms::awaitable{}; }
    } constexpr get_promise{};
} // namespace wtf

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

clu::task<void> tick()
{
    for (auto i = 0_uz;; i++)
    {
        co_await ex::just();
        co_await clutest::wait_on_detached_thread(1s);
        std::cout << "Hi! " << i + 1 << " seconds passed...\n";
    }
}

clu::task<void> canceller()
{
    co_await clutest::wait_on_detached_thread(3141ms);
    std::cout << "Waited 3.141s!\n";
    co_await ex::stop();
}

int main() // NOLINT
{
    clu::static_thread_pool tp(4);
    for (std::size_t i = 0; i < 10; i++)
    {
        clu::this_thread::sync_wait( //
            ex::schedule(tp.get_scheduler()) //
            | ex::then(print_thread_id));
    }
    tp.finish();

    return 0;
}
