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
            explicit promise_env(const in_place_stop_token token): stop_token_(token) {}

        private:
            in_place_stop_token stop_token_;
            friend auto tag_invoke(exec::get_stop_token_t, const promise_env env) noexcept { return env.stop_token_; }
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

        protected:
            std::variant<std::monostate, with_regular_void_t<T>, std::exception_ptr> result_;
            in_place_stop_token stop_token_;

            friend promise_env tag_invoke(exec::get_env_t, const promise<T>& self) noexcept
            {
                return promise_env(self.stop_token_);
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
        concept env_has_stop_token =
            requires { typename exec::stop_token_of_t<exec::env_of_t<T>>; };
        // clang-format on

        template <typename T>
        using env_stop_token_of_t = exec::stop_token_of_t<exec::env_of_t<T>>;

        template <typename T>
        class task_awaitable_base
        {
        public:
            explicit task_awaitable_base(promise<T>& current) noexcept: current_(&current) {}

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
            task_awaitable(promise<T>& current, Parent&) noexcept: task_awaitable_base<T>(current) {}
        };

        // Parent coroutine has an in_place_stop_token,
        // inherit stop token from parent.
        template <typename T, env_has_stop_token Parent>
            requires std::same_as<env_stop_token_of_t<Parent>, in_place_stop_token>
        class task_awaitable<T, Parent> : public task_awaitable_base<T>
        {
        public:
            // Just inherit stop token from parent coroutine
            task_awaitable(promise<T>& current, Parent& parent) noexcept: task_awaitable_base<T>(current)
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
                task_awaitable_base<T>(current),
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
            explicit task_awaitable(promise<T>& current) noexcept: task_awaitable_base<T>(current) {}

            template <typename Parent>
            coro::coroutine_handle<> await_suspend(coro::coroutine_handle<Parent> handle)
            {
                if constexpr (env_has_stop_token<Parent>)
                {
                    Parent& parent = handle.promise();
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
                return task_awaitable_base<T>::await_suspend(handle);
            }

        private:
            in_place_stop_source stop_src_;
            any_unique cb_;
        };

        template <typename T>
        class task_<T>::type : public exec::with_awaitable_senders<promise<T>>
        {
        public:
            using promise_type = promise<T>;

            auto operator co_await() && noexcept { return task_awaitable<T, void>(handle_.get().promise()); }

        private:
            friend promise_base<T>;

            unique_coroutine_handle<promise_type> handle_;

            explicit type(promise_type& promise) noexcept:
                handle_(coro::coroutine_handle<promise_type>::from_promise(promise))
            {
            }

            template <typename P>
            friend auto tag_invoke(exec::as_awaitable_t, type&& self, P& parent_promise)
            {
                return task_awaitable<T, P>(self.handle_.get().promise(), parent_promise);
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
