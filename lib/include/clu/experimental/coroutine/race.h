#pragma once

#include <vector>

#include "concepts.h"
#include "clu/oneway_task.h"
#include "clu/reference.h"
#include "clu/type_traits.h"
#include "clu/experimental/meta/type_list.h"
#include "../outcome.h"

namespace clu
{
    template <typename... Ts>
    class race_tuple_result final
    {
    private:
        template <typename T>
        using real_type = std::conditional_t<std::is_void_v<T>,
            void_tag_t, std::conditional_t<std::is_reference_v<T>,
                std::remove_reference_t<T>*, T>>;

    public:
        using variant_type = std::variant<real_type<Ts>...>;
        static constexpr size_t variant_size = sizeof...(Ts);

    private:
        variant_type data_;

        template <size_t I, typename T>
        decltype(auto) to_real_type(T&& value)
        {
            using type = std::variant_alternative_t<I, variant_type>;
            if constexpr (std::is_void_v<type>)
                return void_tag;
            else if constexpr (std::is_reference_v<type>)
                return std::addressof(value);
            else
                return std::forward<T>(value);
        }

    public:
        race_tuple_result() = default;

        template <size_t I, typename T>
        race_tuple_result(const std::in_place_index_t<I> tag, T&& value):
            data_(tag, to_real_type<I>(std::forward<T>(value))) {}

        size_t index() const noexcept { return data_.index(); }

        variant_type& data() & { return data_; }
        const variant_type& data() const & { return data_; }
        variant_type&& data() && { return std::move(data_); }
        const variant_type&& data() const && { return std::move(data_); }
    };

    template <>
    class race_tuple_result<> final
    {
        static constexpr size_t variant_size = 0;
    };

    // @formatter:off
    template <size_t I, typename T>
        requires (template_of<std::remove_cvref_t<T>, race_tuple_result> 
            && I < std::remove_cvref_t<T>::variant_size)
    // @formatter:on
    decltype(auto) get(T&& value)
    {
        using type = meta::at_t<meta::extract_list_t<std::remove_cvref_t<T>>, I>;
        if constexpr (std::is_void_v<type>)
            return (void)0;
        else if constexpr (std::is_reference_v<type>)
            return static_cast<type>(*std::get<I>(std::forward<T>(value).data()));
        else
            return std::get<I>(std::forward<T>(value).data());
    }

    namespace detail
    {
        template <typename... Ts>
        class race_tuple_awaitable final
        {
        private:
            std::tuple<Ts...> awaitables_;

            template <typename Cvref>
            struct awaiter final
            {
                using parent_t = copy_cvref_t<Cvref, race_tuple_awaitable>;

                template <size_t I>
                using awaitable_t = unwrap_reference_keeping_t<
                    copy_cvref_t<Cvref, std::tuple_element_t<I, std::tuple<Ts...>>>>;

                template <size_t I>
                using result_t = outcome<await_result_t<awaitable_t<I>>>;

                using inter_res_t = std::variant<outcome<await_result_t<
                    unwrap_reference_keeping_t<copy_cvref_t<Cvref, Ts>>>>...>;

                using final_res_t = race_tuple_result<await_result_t<
                    unwrap_reference_keeping_t<copy_cvref_t<Cvref, Ts>>>...>;

                parent_t parent;
                std::atomic_size_t counter = 0;
                inter_res_t inter_res;

                template <size_t I>
                decltype(auto) get_awaitable() const { return static_cast<awaitable_t<I>>(std::get<I>(parent.awaitables_)); }

                template <size_t I, size_t... Is>
                void cancel_rest(std::index_sequence<Is...>) const
                {
                    (((I != Is) ? (void)get_awaitable<Is>().cancel() : (void)0), ...);
                }

                template <size_t I>
                oneway_task await_at(const std::coroutine_handle<> handle)
                {
                    result_t<I> result;
                    try
                    {
                        if constexpr (std::is_void_v<await_result_t<awaitable_t<I>>>)
                        {
                            co_await get_awaitable<I>();
                            result.emplace();
                        }
                        else
                            result.emplace(co_await get_awaitable<I>());
                    }
                    catch (...) { result = std::current_exception(); }
                    const size_t old_counter = counter.fetch_add(1, std::memory_order_acquire);
                    if (old_counter == 0) // First one
                    {
                        inter_res.template emplace<I>(std::move(result));
                        cancel_rest<I>(std::index_sequence_for<Ts...>{});
                    }
                    if (old_counter == sizeof...(Ts) - 1) // Last one
                        handle.resume();
                }

                template <size_t I>
                final_res_t convert_result_at()
                {
                    if constexpr (std::is_void_v<await_result_t<awaitable_t<I>>>)
                        return { std::in_place_index<I>, void_tag };
                    else
                        return { std::in_place_index<I>, *std::get<I>(std::move(inter_res)) };
                }

                template <size_t I = sizeof...(Ts) - 1>
                final_res_t convert_result()
                {
                    if constexpr (I > 0)
                    {
                        if (inter_res.index() == I)
                            return convert_result_at<I>();
                        else
                            return convert_result<I - 1>();
                    }
                    else
                        return convert_result_at<0>();
                }

                bool await_ready() const noexcept { return false; }

                void await_suspend(const std::coroutine_handle<> handle)
                {
                    [=, this]<size_t... Is>(std::index_sequence<Is...>)
                    {
                        (await_at<Is>(handle), ...);
                    }(std::index_sequence_for<Ts...>{});
                }

                final_res_t await_resume() { return convert_result(); }
            };

        public:
            template <typename... Us>
            explicit race_tuple_awaitable(Us&&... awaitables):
                awaitables_(std::forward<Us>(awaitables)...) {}

            awaiter<int&> operator co_await() & { return { *this }; }
            awaiter<const int&> operator co_await() const & { return { *this }; }
            awaiter<int&&> operator co_await() && { return { std::move(*this) }; }
            awaiter<const int&&> operator co_await() const && { return { std::move(*this) }; }
        };

        template <>
        class race_tuple_awaitable<> final
        {
        public:
            bool await_ready() const noexcept { return true; }
            void await_suspend(std::coroutine_handle<>) const noexcept {}
            race_tuple_result<> await_resume() const noexcept { return {}; }
        };

        template <typename... Ts>
        race_tuple_awaitable(Ts ...) -> race_tuple_awaitable<Ts...>;

        template <typename T>
        class race_vector_awaitable final
        {
        private:
            std::vector<T> awaitables_;

            template <typename Cvref>
            struct awaiter final
            {
                using parent_t = copy_cvref_t<Cvref, race_vector_awaitable>;
                using awaitable_t = unwrap_reference_keeping_t<copy_cvref_t<Cvref, T>>;
                using result_t = await_result_t<awaitable_t>;
                using inter_res_t = outcome<result_t>;

                parent_t parent;
                std::atomic_size_t counter = 0;
                std::pair<size_t, inter_res_t> inter_res; // Intermediate result

                decltype(auto) get_awaitable(const size_t i) const { return static_cast<awaitable_t>(parent.awaitables_[i]); }

                void cancel_rest(const size_t i) const
                {
                    for (size_t j = 0; j < parent.awaitables_.size(); j++)
                        if (j != i)
                            get_awaitable(j).cancel();
                }

                oneway_task await_at(const size_t i, const std::coroutine_handle<> handle)
                {
                    inter_res_t result;
                    try
                    {
                        if constexpr (std::is_void_v<result_t>)
                        {
                            co_await static_cast<awaitable_t>(parent.awaitables_[i]);
                            result.emplace();
                        }
                        else
                            result.emplace(co_await static_cast<awaitable_t>(parent.awaitables_[i]));
                    }
                    catch (...) { result = std::current_exception(); }
                    const size_t old_counter = counter.fetch_add(1, std::memory_order_release);
                    if (old_counter == 0) // First one
                    {
                        cancel_rest(i);
                        inter_res.first = i;
                        inter_res.second = std::move(result);
                    }
                    else if (old_counter == parent.awaitables_.size() - 1) // Last one
                        handle.resume();
                }

                bool await_ready() const noexcept { return false; }

                void await_suspend(const std::coroutine_handle<> handle)
                {
                    for (size_t i = 0; i < parent.awaitables_.size(); i++)
                        await_at(i, handle);
                }

                auto await_resume()
                {
                    if constexpr (std::is_void_v<result_t>)
                        return (*inter_res.second, inter_res.first);
                    else
                        return std::pair<size_t, result_t>(inter_res.first, *std::move(inter_res.second));
                }
            };

        public:
            explicit race_vector_awaitable(std::vector<T>&& awaitables):
                awaitables_(std::move(awaitables)) {}

            awaiter<int&> operator co_await() & { return { *this }; }
            awaiter<const int&> operator co_await() const & { return { *this }; }
            awaiter<int&&> operator co_await() && { return { std::move(*this) }; }
            awaiter<const int&&> operator co_await() const && { return { std::move(*this) }; }
        };
    }

    template <typename... Ts> requires (cancellable_awaitable<std::unwrap_reference_t<Ts>> && ...)
    [[nodiscard]] auto race(Ts&&... awaitables)
    {
        return detail::race_tuple_awaitable(std::forward<Ts>(awaitables)...);
    }

    template <typename T> requires cancellable_awaitable<std::unwrap_reference_t<T>>
    [[nodiscard]] auto race(std::vector<T> awaitables)
    {
        return detail::race_vector_awaitable<T>(std::move(awaitables));
    }
}
