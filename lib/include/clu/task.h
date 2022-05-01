#pragma once

#include "execution.h"
#include "stop_token.h"
#include "any_unique.h"
#include "overload.h"
#include "execution/any_scheduler.h"

namespace clu
{
    namespace detail::task
    {
        template <typename T>
        struct task_
        {
            class type;
        };

        template <typename T>
        class promise;

        template <typename T>
        struct final_awaiter
        {
            bool await_ready() const noexcept { return false; }
            coro::coroutine_handle<> await_suspend(coro::coroutine_handle<promise<T>> handle) const noexcept;
            void await_resume() const noexcept {}
        };

        class promise_env
        {
        public:
            // clang-format off
            explicit promise_env(const in_place_stop_token token, const exec::any_scheduler& schd):
                stop_token_(token), schd_(schd) {}
            // clang-format on

        private:
            template <typename T>
            class promise;

            in_place_stop_token stop_token_;
            exec::any_scheduler schd_;

            friend auto tag_invoke(exec::get_stop_token_t, const promise_env& env) noexcept { return env.stop_token_; }
            friend auto tag_invoke(exec::get_scheduler_t, const promise_env& env) noexcept { return env.schd_; }
        };

        template <typename T>
        class promise_base : public exec::with_awaitable_senders<promise<T>>
        {
        public:
            typename task_<T>::type get_return_object() noexcept;
            coro::suspend_always initial_suspend() const noexcept { return {}; }
            final_awaiter<T> final_suspend() const noexcept { return {}; }
            void unhandled_exception() { result_.template emplace<2>(std::current_exception()); }

            void replace_stop_token(const in_place_stop_token token) noexcept { stop_token_ = token; }
            void replace_scheduler(exec::any_scheduler schd) noexcept { schd_ = std::move(schd); }

            template <typename U>
            decltype(auto) await_transform(U&& value)
            {
                auto& pms = static_cast<promise<T>&>(*this);
                if constexpr (exec::no_await_thunk_sender<U>)
                {
                    static_assert(
                        requires { exec::get_completion_scheduler<exec::set_value_t>(value); },
                        "senders configured with no_await_thunk should specify completion schedulers "
                        "using the get_completion_scheduler<set_value_t> customization point");

                    using awaitable_t = call_result_t<exec::as_awaitable_t, U, promise<T>&>;
                    using awaiter_base = exec::awaiter_type_t<awaitable_t, void>;

                    struct awaiter
                    {
                        awaiter_base base;
                        exec::any_scheduler schd;
                        promise<T>& pms;

                        bool await_ready() { return base.await_ready(); }

                        auto await_suspend(coro::coroutine_handle<promise<T>> handle)
                        {
                            return base.await_suspend(handle);
                        }

                        decltype(auto) await_resume()
                        {
                            pms.replace_scheduler(std::move(schd));
                            return base.await_resume();
                        }
                    };

                    return awaiter{
                        exec::get_awaiter(exec::as_awaitable(static_cast<U&&>(value), pms)), //
                        exec::get_completion_scheduler<exec::set_value_t>(value), //
                        pms //
                    };
                }
                else if constexpr (exec::sender<U, promise_env>)
                    return exec::as_awaitable(static_cast<U&&>(value) | exec::transfer(schd_), pms);
                else
                    return exec::as_awaitable(
                        exec::as_awaitable(static_cast<U&&>(value), pms) | exec::transfer(schd_), pms);
            }

        protected:
            std::variant<std::monostate, with_regular_void_t<T>, std::exception_ptr> result_;
            in_place_stop_token stop_token_;
            exec::any_scheduler schd_;

            friend promise_env tag_invoke(exec::get_env_t, const promise<T>& self) noexcept
            {
                return promise_env(self.stop_token_, self.schd_);
            }
        };

        template <typename T>
        class promise : public promise_base<T>
        {
        public:
            template <std::convertible_to<T> U = T>
            void return_value(U&& value)
            {
                this->result_.template emplace<1>(static_cast<U&&>(value));
            }

            T get_result() &&
            {
                return std::visit( //
                    clu::overload( //
                        [](std::monostate) -> T { unreachable(); }, //
                        [](T&& result) -> T { return static_cast<T&&>(result); }, //
                        [](const std::exception_ptr& eptr) -> T { std::rethrow_exception(eptr); }),
                    std::move(this->result_));
            }
        };

        template <>
        class promise<void> : public promise_base<void>
        {
        public:
            void return_void() { result_.emplace<1>(); }

            void get_result() &&
            {
                std::visit( //
                    overload( //
                        [](std::monostate) { unreachable(); }, //
                        [](unit) {}, //
                        [](const std::exception_ptr& eptr) { std::rethrow_exception(eptr); }),
                    std::move(result_));
            }
        };

        template <typename T>
        coro::coroutine_handle<> final_awaiter<T>::await_suspend(
            coro::coroutine_handle<promise<T>> handle) const noexcept
        {
            return handle.promise().continuation();
        }

        // clang-format off
        template <typename T>
        concept env_has_scheduler = callable<exec::get_scheduler_t, exec::env_of_t<T>>;
        template <typename T>
        concept env_has_stop_token = requires { typename exec::stop_token_of_t<exec::env_of_t<T>>; };
        // clang-format on

        template <typename T>
        using env_stop_token_of_t = exec::stop_token_of_t<exec::env_of_t<T>>;

        template <typename T>
        class task_awaitable_base
        {
        public:
            template <typename P>
            explicit task_awaitable_base(promise<T>& current, P&& parent) noexcept: current_(&current)
            {
                if constexpr (env_has_scheduler<P>)
                    current_->replace_scheduler(exec::get_scheduler(exec::get_env(parent)));
                else
                    current_->replace_scheduler(exec::trampoline_scheduler{});
            }

            bool await_ready() const noexcept { return false; }

            template <typename Parent>
            coro::coroutine_handle<> await_suspend(coro::coroutine_handle<Parent> handle) const noexcept
            {
                current_->set_continuation(handle);
                return coro::coroutine_handle<promise<T>>::from_promise(*current_);
            }

            T await_resume() const { return std::move(*current_).get_result(); }

        protected:
            promise<T>* current_ = nullptr;
        };

        struct stop_propagation_callback
        {
            in_place_stop_source& stop_src;
            void operator()() const noexcept { stop_src.request_stop(); }
        };

        // Stop token propagation of tasks is implemented in two separate ways:
        // 1. If the parent coroutine promise derives from with_awaitable_senders,
        // we can customize the awaitable based on the parent promise type, since
        // with_awaitable_senders implements await_transform by calling as_awaitable.
        // 2. If the parent is just some random coroutine, we cannot specialize
        // operator co_await to the promise type, thus we can only store type erased
        // stop_callback in our awaitable.
        // Primary template is for when parent coroutine has no stop_token mechanics.
        template <typename T, typename Parent = void>
        class task_awaitable : public task_awaitable_base<T>
        {
        public:
            task_awaitable(promise<T>& current, Parent& parent) noexcept: task_awaitable_base<T>(current, parent) {}
        };

        // Parent coroutine has an in_place_stop_token,
        // inherit stop token from parent.
        template <typename T, env_has_stop_token Parent>
            requires std::same_as<env_stop_token_of_t<Parent>, in_place_stop_token>
        class task_awaitable<T, Parent> : public task_awaitable_base<T>
        {
        public:
            // Just inherit stop token from parent coroutine
            task_awaitable(promise<T>& current, Parent& parent) noexcept: task_awaitable_base<T>(current, parent)
            {
                current.replace_stop_token(exec::get_stop_token(exec::get_env(parent)));
            }
        };

        // Parent coroutine has a stoppable token but not in_place_stop_token,
        // use a callback to propagate the stop signal.
        template <typename T, env_has_stop_token Parent>
            requires same_as_none_of<env_stop_token_of_t<Parent>, in_place_stop_token, never_stop_token>
        class task_awaitable<T, Parent> : public task_awaitable_base<T>
        {
        public:
            task_awaitable(promise<T>& current, Parent& parent) noexcept:
                task_awaitable_base<T>(current, parent),
                cb_(exec::get_stop_token(exec::get_env(parent)), stop_propagation_callback{stop_src_})
            {
                current.replace_stop_token(stop_src_.get_token());
            }

        private:
            using stop_token_t = env_stop_token_of_t<Parent>;
            using stop_callback_t = typename stop_token_t::template callback_type<stop_propagation_callback>;

            in_place_stop_source stop_src_;
            stop_callback_t cb_;
        };

        // Created via co_await, worst case scenario.
        // We can only get the type of the parent promise in await_suspend,
        // so we do stop token things there, but type erasure is absolutely needed.
        template <typename T>
        class task_awaitable<T, void> : public task_awaitable_base<T>
        {
        public:
            // clang-format off
            explicit task_awaitable(promise<T>& current) noexcept: //
                task_awaitable_base<T>(current, 0) {} // 0 is not even an environment, default scheduler will be used
            // clang-format on

            template <typename Parent>
            coro::coroutine_handle<> await_suspend(coro::coroutine_handle<Parent> handle)
            {
                Parent& parent = handle.promise();
                if constexpr (env_has_stop_token<Parent>)
                {
                    using stop_token_t = env_stop_token_of_t<Parent>;
                    if constexpr (std::is_same_v<stop_token_t, in_place_stop_token>)
                        this->current_->replace_stop_token(exec::get_stop_token(exec::get_env(parent)));
                    else if constexpr (!std::is_same_v<stop_token_t, never_stop_token>)
                    {
                        this->current_->replace_stop_token(stop_src_.get_token());
                        using callback_t = typename stop_token_t::template callback_type<stop_propagation_callback>;
                        cb_.emplace(std::in_place_type<callback_t>, exec::get_stop_token(exec::get_env(parent)),
                            stop_propagation_callback{stop_src_});
                    }
                }
                if constexpr (env_has_scheduler<Parent>)
                    this->current_->replace_scheduler(exec::get_scheduler(exec::get_env(parent)));
                return task_awaitable_base<T>::await_suspend(handle);
            }

        private:
            in_place_stop_source stop_src_;
            any_unique cb_;
        };

        template <typename T>
        class task_<T>::type
        {
        public:
            using promise_type = promise<T>;

            auto operator co_await() && noexcept { return task_awaitable<T, void>(handle_.get().promise()); }

        private:
            friend promise_base<T>;

            unique_coroutine_handle<promise_type> handle_;

            // clang-format off
            explicit type(promise_type& promise) noexcept:
                handle_(coro::coroutine_handle<promise_type>::from_promise(promise)) {}
            // clang-format on

            template <typename P>
            friend auto tag_invoke(exec::as_awaitable_t, type&& self, P& parent_promise)
            {
                return task_awaitable<T, P>( //
                    self.handle_.get().promise(), parent_promise);
            }
        };

        template <typename T>
        typename task_<T>::type promise_base<T>::get_return_object() noexcept
        {
            using result_t = typename task_<T>::type;
            return result_t(static_cast<promise<T>&>(*this));
        }
    } // namespace detail::task

    template <typename T>
    using task = typename detail::task::task_<T>::type;
} // namespace clu
