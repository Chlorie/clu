#pragma once

#include "task.h"

namespace clu
{
    namespace detail::flow
    {
        template <typename T>
        struct flow_
        {
            class type;
        };

        template <typename T>
        class promise : public coro_pms::promise_base<T, promise<T>>
        {
        public:
            coro::suspend_always final_suspend() const noexcept { return {}; }
            typename flow_<T>::type get_return_object() noexcept;

            void unhandled_exception()
            {
                using base = coro_pms::promise_base<T, promise<T>>;
                base::unhandled_exception(); // Put the exception into the variant first
                this->continuation().resume(); // Manually return to the waiting coroutine
            }

            // Send `stopped` (indicating the end of the stream) to the waiting coroutine
            void return_void() const noexcept { (void)this->unhandled_stopped(); }

            // Save the value and symmetric transfer to the waiting coroutine
            template <std::convertible_to<T> U = T>
            auto yield_value(U&& value)
            {
                this->result_.template emplace<1>(static_cast<U&&>(value));
                struct awaiter_t
                {
                    coro::coroutine_handle<> waiting{};
                    bool await_ready() const noexcept { return false; }
                    auto await_suspend(coro::coroutine_handle<>) const noexcept { return waiting; }
                    void await_resume() const noexcept {}
                };
                return awaiter_t{this->continuation()};
            }
        };

        template <typename T, typename Parent>
        class next_awaiter : public coro_pms::awaiter_base<T, promise<T>, Parent>
        {
        public:
            using base = coro_pms::awaiter_base<T, promise<T>, Parent>;
            using base::base;

            T await_resume() const { return this->current_->get_result(); }
        };

        template <typename T>
        struct next_awt_t_
        {
            class type;
        };

        template <typename T>
        using next_awaitable = typename next_awt_t_<T>::type;

        template <typename T>
        class next_awt_t_<T>::type
        {
        public:
            explicit type(promise<T>* promise) noexcept: promise_(promise) {}
            auto operator co_await() && { return next_awaiter<T, void>(*promise_); }

        private:
            promise<T>* promise_ = nullptr;

            template <typename P>
            friend auto tag_invoke(exec::as_awaitable_t, type&& self, P& parent_promise)
            {
                return next_awaiter<T, P>(*self.promise_, parent_promise);
            }
        };

        template <typename T>
        class flow_<T>::type
        {
        public:
            using promise_type = promise<T>;

        private:
            friend promise<T>;

            unique_coroutine_handle<promise_type> handle_;

            // clang-format off
            explicit type(promise_type& pms) noexcept:
                handle_(coro::coroutine_handle<promise_type>::from_promise(pms)) {}
            // clang-format on

            friend auto tag_invoke(exec::next_t, type& self) noexcept
            {
                return next_awaitable<T>(&self.handle_.promise());
            }

            // TODO: support attaching custom cleanup tasks
        };

        template <typename T>
        typename flow_<T>::type promise<T>::get_return_object() noexcept
        {
            using result_t = typename flow_<T>::type;
            return result_t(*this);
        }
    } // namespace detail::flow

    template <typename T>
    using flow = typename detail::flow::flow_<T>::type;
} // namespace clu
