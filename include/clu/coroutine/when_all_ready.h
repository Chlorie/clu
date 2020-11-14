#pragma once

#include <vector>

#include "concepts.h"
#include "oneway_task.h"
#include "../outcome.h"
#include "../reference.h"
#include "../meta.h"

namespace clu
{
    namespace detail
    {
        template <typename... Ts> class when_all_ready_tuple_awaitable;
        template <typename... Ts> struct when_all_ready_tuple_awaiter_base;

        template <typename T> class when_all_ready_vector_awaitable;
        template <typename T> struct when_all_ready_vector_awaiter_base;
    }

    template <typename... Ts>
    class when_all_ready_tuple_result final
    {
        friend struct detail::when_all_ready_tuple_awaiter_base<Ts...>;
    public:
        using result_type = std::tuple<outcome<await_result_t<unwrap_reference_t<Ts>&>>...>;

    private:
        std::tuple<Ts...> awaitables_;
        result_type results_;

    public:
        template <typename... Us>
        explicit when_all_ready_tuple_result(Us&&... awaitables):
            awaitables_(std::forward<Us>(awaitables)...) {}

        template <size_t I, typename Self>
        friend auto&& get(Self&& self)
        {
            return std::get<I>(
                static_cast<copy_cvref_t<Self&&, result_type>>(self.results_));
        }
    };

    template <typename T>
    class when_all_ready_vector_result final
    {
        friend struct detail::when_all_ready_vector_awaiter_base<T>;
    public:
        using outcome_type = outcome<await_result_t<unwrap_reference_t<T>&>>;
        using result_type = std::vector<outcome_type>;
        using iterator = typename result_type::iterator;
        using const_iterator = typename result_type::const_iterator;
        using reverse_iterator = typename result_type::reverse_iterator;
        using const_reverse_iterator = typename result_type::const_reverse_iterator;

    private:
        std::vector<T> awaitables_;
        result_type results_;

    public:
        explicit when_all_ready_vector_result(std::vector<T>&& awaitables):
            awaitables_(std::move(awaitables)), results_(awaitables_.size()) {}

        auto begin() { return results_.begin(); }
        auto begin() const { return results_.begin(); }
        auto cbegin() const { return results_.cbegin(); }
        auto end() { return results_.end(); }
        auto end() const { return results_.end(); }
        auto cend() const { return results_.cend(); }
        auto rbegin() { return results_.rbegin(); }
        auto rbegin() const { return results_.rbegin(); }
        auto crbegin() const { return results_.crbegin(); }
        auto rend() { return results_.rend(); }
        auto rend() const { return results_.rend(); }
        auto crend() const { return results_.crend(); }

        outcome_type& front() { return results_.front(); }
        const outcome_type& front() const { return results_.front(); }
        outcome_type& back() { return results_.back(); }
        const outcome_type& back() const { return results_.back(); }
        outcome_type* data() { return results_.data(); }
        const outcome_type* data() const { return results_.data(); }

        outcome_type& operator[](const size_t index) { return results_[index]; }
        const outcome_type& operator[](const size_t index) const { return results_[index]; }
        outcome_type& at(const size_t index) { return results_.at(index); }
        const outcome_type& at(const size_t index) const { return results_.at(index); }

        bool empty() const noexcept { return results_.empty(); }
        size_t size() const noexcept { return results_.size(); }
    };

    namespace detail
    {
        template <typename... Ts>
        class when_all_ready_tuple_awaitable final
        {
            friend struct when_all_ready_tuple_awaiter_base<Ts...>;
        private:
            using awaiter_base = when_all_ready_tuple_awaiter_base<Ts...>;
            using result_type = when_all_ready_tuple_result<Ts...>;
            result_type result_;

        public:
            template <typename... Us>
            explicit when_all_ready_tuple_awaitable(Us&&... awaitables):
                result_(std::forward<Us>(awaitables)...) {}

            auto operator co_await() &;
            auto operator co_await() &&;
        };

        template <typename... Ts>
        struct when_all_ready_tuple_awaiter_base
        {
            when_all_ready_tuple_awaitable<Ts...>& parent;
            std::atomic_size_t counter = sizeof...(Ts);

            template <size_t I>
            oneway_task await_at(const std::coroutine_handle<> handle)
            {
                auto& result = std::get<I>(parent.result_.results_);
                auto& awaitable = unwrap(std::get<I>(parent.result_.awaitables_));
                try { result.emplace(co_await awaitable); }
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

            auto& result() const { return parent.result_; }
        };

        template <typename ... Ts>
        auto when_all_ready_tuple_awaitable<Ts...>::operator co_await() &
        {
            struct awaiter : awaiter_base
            {
                result_type& await_resume() { return this->result(); }
            };
            return awaiter{ { *this } };
        }

        template <typename ... Ts>
        auto when_all_ready_tuple_awaitable<Ts...>::operator co_await() &&
        {
            struct awaiter : awaiter_base
            {
                result_type&& await_resume() { return std::move(this->result()); }
            };
            return awaiter{ { *this } };
        }

        template <>
        class when_all_ready_tuple_awaitable<> final
        {
        public:
            bool await_ready() const noexcept { return true; }
            void await_suspend(std::coroutine_handle<>) const noexcept {}
            std::tuple<> await_resume() const noexcept { return {}; }
        };

        template <typename T>
        class when_all_ready_vector_awaitable final
        {
            friend struct when_all_ready_vector_awaiter_base<T>;
        private:
            using awaiter_base = when_all_ready_vector_awaiter_base<T>;
            using result_type = when_all_ready_vector_result<T>;
            result_type result_;

        public:
            template <typename... Us>
            explicit when_all_ready_vector_awaitable(Us&&... awaitables):
                result_(std::forward<Us>(awaitables)...) {}

            auto operator co_await() &;
            auto operator co_await() &&;
        };

        template <typename T>
        struct when_all_ready_vector_awaiter_base
        {
            when_all_ready_vector_awaitable<T>& parent;
            std::atomic_size_t counter = parent.result_.size();

            oneway_task await_at(const size_t i, const std::coroutine_handle<> handle)
            {
                auto& result = parent.result_.results_[i];
                auto& awaitable = unwrap(parent.result_.awaitables_[i]);
                try { result.emplace(co_await awaitable); }
                catch (...) { result = std::current_exception(); }
                if (counter.fetch_sub(1, std::memory_order_acq_rel) == 1) handle.resume();
            }

            bool await_ready() const noexcept { return false; }

            void await_suspend(const std::coroutine_handle<> handle)
            {
                for (size_t i = 0; i < parent.result_.size(); i++)
                    await_at(i, handle);
            }

            auto& result() const { return parent.result_; }
        };

        template <typename T>
        auto when_all_ready_vector_awaitable<T>::operator co_await() &
        {
            struct awaiter : awaiter_base
            {
                result_type& await_resume() { return this->result(); }
            };
            return awaiter{ { *this } };
        }

        template <typename T>
        auto when_all_ready_vector_awaitable<T>::operator co_await() &&
        {
            struct awaiter : awaiter_base
            {
                result_type&& await_resume() { return std::move(this->result()); }
            };
            return awaiter{ { *this } };
        }
    }

    template <typename... Ts> requires (awaitable<unwrap_reference_t<Ts>> && ...)
    [[nodiscard]] auto when_all_ready(Ts&&... awaitables)
    {
        return detail::when_all_ready_tuple_awaitable<std::remove_cvref_t<Ts>...>
            (std::forward<Ts>(awaitables)...);
    }

    template <awaitable T>
    [[nodiscard]] auto when_all_ready(std::vector<T> awaitables)
    {
        return detail::when_all_ready_vector_awaitable<T>(std::move(awaitables));
    }
}

template <typename... Ts>
struct std::tuple_size<clu::when_all_ready_tuple_result<Ts...>>
{
    static constexpr size_t value = sizeof...(Ts);
};

template <size_t I, typename... Ts>
struct std::tuple_element<I, clu::when_all_ready_tuple_result<Ts...>>
{
    using type = std::tuple_element_t<I,
        typename clu::when_all_ready_tuple_result<Ts...>::result_type>;
};
