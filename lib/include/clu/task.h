#pragma once

#include "execution.h"
#include "stop_token.h"
#include "any_unique.h"
#include "overload.h"
#include "execution/any_scheduler.h"

namespace clu
{
    namespace detail
    {
        namespace coro_pms
        {
            class env_t
            {
            public:
                // clang-format off
                explicit env_t(const in_place_stop_token token, const exec::any_scheduler& schd):
                    stop_token_(token), schd_(schd) {}
                // clang-format on

            private:
                in_place_stop_token stop_token_;
                exec::any_scheduler schd_;

                friend auto tag_invoke(get_stop_token_t, const env_t& env) noexcept { return env.stop_token_; }
                friend auto tag_invoke(exec::get_scheduler_t, const env_t& env) noexcept { return env.schd_; }
            };

            template <typename P, typename U>
            decltype(auto) task_await_transform(P& pms, U&& value)
            {
                if constexpr (exec::no_await_thunk_sender<U>)
                {
                    static_assert(
                        requires { exec::get_completion_scheduler<exec::set_value_t>(get_env(value)); },
                        "senders configured with no_await_thunk should specify completion schedulers "
                        "using the get_completion_scheduler<set_value_t> customization point on their environments");

                    using awaitable_t = call_result_t<exec::as_awaitable_t, U, P&>;
                    const auto get_awaitable = [&] { return exec::as_awaitable(static_cast<U&&>(value), pms); };
                    const auto get_awaiter = [&]
                    {
                        if constexpr (exec::awaiter<awaitable_t, P>)
                            return get_awaitable();
                        else
                            return exec::get_awaiter(get_awaitable());
                    };
                    using awaiter_base = call_result_t<decltype(get_awaiter)>;

                    struct awaiter
                    {
                        awaiter_base base;
                        exec::any_scheduler schd;
                        P& pms;

                        bool await_ready() { return base.await_ready(); }

                        auto await_suspend(coro::coroutine_handle<P> handle) { return base.await_suspend(handle); }

                        decltype(auto) await_resume()
                        {
                            pms.replace_scheduler(std::move(schd));
                            return base.await_resume();
                        }
                    };

                    auto schd = exec::get_completion_scheduler<exec::set_value_t>(get_env(value));
                    return awaiter{get_awaiter(), std::move(schd), pms};
                }
                else
                {
                    auto schd = exec::get_scheduler(get_env(pms));
                    if constexpr (exec::sender_in<U, env_t>)
                        return exec::as_awaitable(static_cast<U&&>(value) | exec::transfer(std::move(schd)), pms);
                    else
                        return exec::as_awaitable(
                            exec::as_awaitable(static_cast<U&&>(value), pms) | exec::transfer(std::move(schd)), pms);
                }
            }

            template <typename T, typename P>
            struct pms_base_t_
            {
                class type;
            };

            template <typename T, typename P>
            using promise_base = typename pms_base_t_<T, P>::type;

            template <typename T, typename P>
            class pms_base_t_<T, P>::type : public exec::with_awaitable_senders<P>
            {
            public:
                coro::suspend_always initial_suspend() const noexcept { return {}; }
                void unhandled_exception() { result_.template emplace<2>(std::current_exception()); }

                void replace_stop_token(const in_place_stop_token token) noexcept { stop_token_ = token; }
                void replace_scheduler(exec::any_scheduler schd) noexcept { schd_ = std::move(schd); }

                template <typename U>
                decltype(auto) await_transform(U&& value)
                {
                    return coro_pms::task_await_transform( //
                        static_cast<P&>(*this), static_cast<U&&>(value));
                }

                T get_result()
                {
                    if constexpr (std::is_void_v<T>)
                    {
                        return std::visit( //
                            clu::overload( //
                                [](std::monostate) { unreachable(); }, //
                                [](unit) {}, //
                                [](const std::exception_ptr& eptr) { std::rethrow_exception(eptr); }),
                            std::move(result_));
                    }
                    else
                    {
                        return std::visit( //
                            clu::overload( //
                                [](std::monostate) -> T { unreachable(); }, //
                                [](T&& result) -> T { return static_cast<T&&>(result); }, //
                                [](const std::exception_ptr& eptr) -> T { std::rethrow_exception(eptr); }),
                            std::move(this->result_));
                    }
                }

            protected:
                std::variant<std::monostate, with_regular_void_t<T>, std::exception_ptr> result_;
                in_place_stop_token stop_token_;
                exec::any_scheduler schd_;

                friend env_t tag_invoke(get_env_t, const P& self) noexcept
                {
                    return env_t(self.stop_token_, self.schd_);
                }
            };

            // clang-format off
            template <typename T>
            concept env_has_scheduler = callable<exec::get_scheduler_t, env_of_t<T>>;
            template <typename T>
            concept env_has_stop_token = requires { typename stop_token_of_t<env_of_t<T>>; };
            // clang-format on

            template <typename T>
            using env_stop_token_of_t = stop_token_of_t<env_of_t<T>>;

            template <typename T, typename P>
            class awaiter_schd_base
            {
            public:
                template <typename Parent>
                explicit awaiter_schd_base(P& current, Parent&& parent) noexcept: current_(&current)
                {
                    if constexpr (env_has_scheduler<Parent>)
                        current_->replace_scheduler(exec::get_scheduler(get_env(parent)));
                    else
                        current_->replace_scheduler(exec::trampoline_scheduler{});
                }

                bool await_ready() const noexcept { return false; }

                template <typename Parent>
                coro::coroutine_handle<> await_suspend(coro::coroutine_handle<Parent> handle) const noexcept
                {
                    current_->set_continuation(handle);
                    return coro::coroutine_handle<P>::from_promise(*current_);
                }

            protected:
                P* current_ = nullptr;
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
            template <typename T, typename P, typename Parent = void>
            class awaiter_base : public awaiter_schd_base<T, P>
            {
            public:
                awaiter_base(P& current, Parent& parent) noexcept: awaiter_schd_base<T, P>(current, parent) {}
            };

            // Parent coroutine has an in_place_stop_token,
            // inherit stop token from parent.
            template <typename T, typename P, env_has_stop_token Parent>
                requires std::same_as<env_stop_token_of_t<Parent>, in_place_stop_token>
            class awaiter_base<T, P, Parent> : public awaiter_schd_base<T, P>
            {
            public:
                // Just inherit stop token from parent coroutine
                awaiter_base(P& current, Parent& parent) noexcept: awaiter_schd_base<T, P>(current, parent)
                {
                    current.replace_stop_token(get_stop_token(get_env(parent)));
                }
            };

            // Parent coroutine has a stoppable token but not in_place_stop_token,
            // use a callback to propagate the stop signal.
            template <typename T, typename P, env_has_stop_token Parent>
                requires same_as_none_of<env_stop_token_of_t<Parent>, in_place_stop_token, never_stop_token>
            class awaiter_base<T, P, Parent> : public awaiter_schd_base<T, P>
            {
            public:
                awaiter_base(P& current, Parent& parent) noexcept:
                    awaiter_schd_base<T, P>(current, parent),
                    cb_(get_stop_token(get_env(parent)), stop_propagation_callback{stop_src_})
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
            template <typename T, typename P>
            class awaiter_base<T, P, void> : public awaiter_schd_base<T, P>
            {
            public:
                // clang-format off
                explicit awaiter_base(P& current) noexcept: //
                    awaiter_schd_base<T, P>(current, 0) {}
                // 0 is not even an environment, default scheduler will be used
                // clang-format on

                template <typename Parent>
                coro::coroutine_handle<> await_suspend(coro::coroutine_handle<Parent> handle)
                {
                    Parent& parent = handle.promise();
                    if constexpr (env_has_stop_token<Parent>)
                    {
                        using stop_token_t = env_stop_token_of_t<Parent>;
                        if constexpr (std::is_same_v<stop_token_t, in_place_stop_token>)
                            this->current_->replace_stop_token(get_stop_token(get_env(parent)));
                        else if constexpr (!std::is_same_v<stop_token_t, never_stop_token>)
                        {
                            this->current_->replace_stop_token(stop_src_.get_token());
                            using callback_t = typename stop_token_t::template callback_type<stop_propagation_callback>;
                            cb_.emplace(std::in_place_type<callback_t>, get_stop_token(get_env(parent)),
                                stop_propagation_callback{stop_src_});
                        }
                    }
                    if constexpr (env_has_scheduler<Parent>)
                        this->current_->replace_scheduler(exec::get_scheduler(get_env(parent)));
                    return awaiter_schd_base<T, P>::await_suspend(handle);
                }

            private:
                in_place_stop_source stop_src_;
                any_unique cb_;
            };
        } // namespace coro_pms

        namespace task
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

            template <typename T>
            class promise_base : public coro_pms::promise_base<T, promise<T>>
            {
            public:
                typename task_<T>::type get_return_object() noexcept;
                final_awaiter<T> final_suspend() const noexcept { return {}; }
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
            };

            template <>
            class promise<void> : public promise_base<void>
            {
            public:
                void return_void() { result_.emplace<1>(); }
            };

            template <typename T>
            coro::coroutine_handle<> final_awaiter<T>::await_suspend(
                coro::coroutine_handle<promise<T>> handle) const noexcept
            {
                return handle.promise().continuation();
            }

            template <typename T, typename Parent>
            class task_awaiter : public coro_pms::awaiter_base<T, promise<T>, Parent>
            {
            public:
                using base = coro_pms::awaiter_base<T, promise<T>, Parent>;
                using base::base;

                T await_resume() const { return this->current_->get_result(); }
            };

            template <typename T>
            class task_<T>::type
            {
            public:
                using promise_type = promise<T>;
                auto operator co_await() && noexcept { return task_awaiter<T, void>(handle_.get().promise()); }

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
                    return task_awaiter<T, P>(self.handle_.get().promise(), parent_promise);
                }
            };

            template <typename T>
            typename task_<T>::type promise_base<T>::get_return_object() noexcept
            {
                using result_t = typename task_<T>::type;
                return result_t(static_cast<promise<T>&>(*this));
            }

            template <typename S>
            using task_return_type_of = exec::value_types_of_t<S, coro_pms::env_t, //
                exec::detail::collapse_types_t, exec::detail::collapse_types_t>;

            template <typename S>
            concept task_convertible_sender = exec::sender<S> && requires { typename task_return_type_of<S>; };
        } // namespace task
    } // namespace detail

    template <typename T>
    using task = typename detail::task::task_<T>::type;

    struct as_task_t
    {
        template <exec::sender S>
        CLU_STATIC_CALL_OPERATOR(auto)
        (S&& sender)
        {
            static_assert(detail::task::task_convertible_sender<S>,
                "Senders that send multiple sets of values, or those that send "
                "multiple values in some single set are not convertible into tasks. "
                "For those that send multiple value sets, consider using "
                "clu::exec::into_variant(sender) to convert the multiple sets "
                "into a single value; for those that send multiple values, "
                "consider using clu::exec::into_tuple(sender) to collapse "
                "the values into a tuple.");
            using return_type = detail::task::task_return_type_of<S>;
            if constexpr (std::is_same_v<task<return_type>, S>)
                return static_cast<S&&>(sender);
            else
                return [](S&& snd) -> task<return_type>
                { co_return co_await static_cast<S&&>(snd); }(static_cast<S&&>(sender));
        }

        constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(*this); }
    } inline constexpr as_task{};
} // namespace clu
