#pragma once

#include <vector>

#include "concepts.h"
#include "clu/oneway_task.h"
#include "clu/outcome.h"
#include "clu/reference.h"
#include "clu/type_traits.h"

namespace clu
{
    namespace detail
    {
        template <typename... Ts>
        class when_all_ready_tuple_awaitable final
        {
        private:
            std::tuple<Ts...> awaitables_;

            template <typename Cvref>
            struct awaiter final
            {
                using parent_t = copy_cvref_t<Cvref, when_all_ready_tuple_awaitable>;

                template <size_t I>
                using awaitable_t = unwrap_reference_keeping_t<
                    copy_cvref_t<Cvref, std::tuple_element_t<I, std::tuple<Ts...>>>>;

                template <size_t I>
                using result_t = outcome<await_result_t<awaitable_t<I>>>;

                parent_t parent;
                std::atomic_size_t counter = sizeof...(Ts);
                std::tuple<outcome<await_result_t<
                    unwrap_reference_keeping_t<copy_cvref_t<Cvref, Ts>>>>...> results;

                template <size_t I>
                oneway_task await_at(const std::coroutine_handle<> handle)
                {
                    auto& result = std::get<I>(results);
                    try
                    {
                        if constexpr (std::is_void_v<await_result_t<awaitable_t<I>>>)
                        {
                            co_await static_cast<awaitable_t<I>>(std::get<I>(parent.awaitables_));
                            result.emplace();
                        }
                        else
                            result.emplace(co_await static_cast<awaitable_t<I>>(std::get<I>(parent.awaitables_)));
                    }
                    catch (...) { result = std::current_exception(); }
                    if (counter.fetch_sub(1, std::memory_order_acq_rel) == 1) handle.resume();
                }

                bool await_ready() const noexcept { return false; }

                void await_suspend(const std::coroutine_handle<> handle)
                {
                    [=, this]<size_t... Is>(std::index_sequence<Is...>)
                    {
                        (await_at<Is>(handle), ...);
                    }(std::index_sequence_for<Ts...>{});
                }

                auto await_resume() { return std::move(results); }
            };

        public:
            template <typename... Us>
            explicit when_all_ready_tuple_awaitable(Us&&... awaitables):
                awaitables_(std::forward<Us>(awaitables)...) {}

            awaiter<int&> operator co_await() & { return { *this }; }
            awaiter<const int&> operator co_await() const & { return { *this }; }
            awaiter<int&&> operator co_await() && { return { std::move(*this) }; }
            awaiter<const int&&> operator co_await() const && { return { std::move(*this) }; }
        };

        template <>
        class when_all_ready_tuple_awaitable<> final
        {
        public:
            bool await_ready() const noexcept { return true; }
            void await_suspend(std::coroutine_handle<>) const noexcept {}
            std::tuple<> await_resume() const noexcept { return {}; }
        };

        template <typename... Ts>
        when_all_ready_tuple_awaitable(Ts ...) -> when_all_ready_tuple_awaitable<Ts...>;

        template <typename T>
        class when_all_ready_vector_awaitable final
        {
        private:
            std::vector<T> awaitables_;

            template <typename Cvref>
            struct awaiter final
            {
                using parent_t = copy_cvref_t<Cvref, when_all_ready_vector_awaitable>;
                using awaitable_t = unwrap_reference_keeping_t<copy_cvref_t<Cvref, T>>;
                using result_t = outcome<await_result_t<awaitable_t>>;

                parent_t parent;
                std::atomic_size_t counter = parent.awaitables_.size();
                std::vector<result_t> results = std::vector<result_t>(parent.awaitables_.size());

                oneway_task await_at(const size_t i, const std::coroutine_handle<> handle)
                {
                    auto& result = results[i];
                    try
                    {
                        if constexpr (std::is_void_v<await_result_t<awaitable_t>>)
                        {
                            co_await static_cast<awaitable_t>(parent.awaitables_[i]);
                            result.emplace();
                        }
                        else
                            result.emplace(co_await static_cast<awaitable_t>(parent.awaitables_[i]));
                    }
                    catch (...) { result = std::current_exception(); }
                    if (counter.fetch_sub(1, std::memory_order_acq_rel) == 1) handle.resume();
                }

                bool await_ready() const noexcept { return false; }

                void await_suspend(const std::coroutine_handle<> handle)
                {
                    for (size_t i = 0; i < results.size(); i++)
                        await_at(i, handle);
                }

                auto await_resume() { return std::move(results); }
            };

        public:
            explicit when_all_ready_vector_awaitable(std::vector<T>&& awaitables):
                awaitables_(std::move(awaitables)) {}

            awaiter<int&> operator co_await() & { return { *this }; }
            awaiter<const int&> operator co_await() const & { return { *this }; }
            awaiter<int&&> operator co_await() && { return { std::move(*this) }; }
            awaiter<const int&&> operator co_await() const && { return { std::move(*this) }; }
        };
    }

    template <typename... Ts> requires (awaitable<std::unwrap_reference_t<Ts>> && ...)
    [[nodiscard]] auto when_all_ready(Ts&&... awaitables)
    {
        return detail::when_all_ready_tuple_awaitable(std::forward<Ts>(awaitables)...);
    }

    template <awaitable T>
    [[nodiscard]] auto when_all_ready(std::vector<T> awaitables)
    {
        return detail::when_all_ready_vector_awaitable<T>(std::move(awaitables));
    }
}
