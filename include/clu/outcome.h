#pragma once

#include <variant>

#include "concepts.h"
#include "overload.h"

namespace clu
{
    struct exceptional_outcome_t {};
    inline constexpr exceptional_outcome_t exceptional_outcome{};

    struct void_tag_t {};
    inline constexpr void_tag_t void_tag{};

    class bad_outcome_access final : public std::exception
    {
    public:
        const char* what() const override { return "bad outcome access"; }
    };

    namespace detail
    {
        template <typename T>
        class outcome_base
        {
        protected:
            std::variant<std::monostate, T, std::exception_ptr> value_;

        public:
            outcome_base() = default;
            explicit(false) outcome_base(const T& value): value_(value) {}
            explicit(false) outcome_base(T&& value): value_(std::move(value)) {}
            outcome_base(exceptional_outcome_t): value_(std::current_exception()) {}
            outcome_base(exceptional_outcome_t, const std::exception_ptr& eptr): value_(eptr) {}

            template <typename... Ts> requires std::constructible_from<T, Ts...>
            explicit outcome_base(std::in_place_t, Ts&&... args): value_(std::in_place_index<1>, std::forward<Ts>(args)...) {}

            bool empty() const noexcept { return value_.index() == 0; }
            bool has_value() const noexcept { return value_.index() == 1; }
            bool has_exception() const noexcept { return value_.index() == 2; }
            explicit operator bool() const noexcept { return value_.index() == 1; }

            std::exception_ptr exception() const noexcept
            {
                if (const auto* eptrptr = std::get_if<2>(&value_))
                    return *eptrptr;
                return {};
            }

            void throw_if_exceptional() const
            {
                if (const auto* eptrptr = std::get_if<2>(&value_))
                    std::rethrow_exception(*eptrptr);
            }
        };
    }

    // TODO: functional interface
    // TODO: use outcome to refactor coroutine utils

    template <typename T>
    class outcome final : public detail::outcome_base<T>
    {
    private:
        template <typename U>
        static decltype(auto) get_impl(U&& value)
        {
            using result_t = decltype(std::get<T>(std::forward<U>(value).value_));
            return std::visit(overload
                {
                    [](std::monostate) -> result_t { throw bad_outcome_access(); },
                    [](const std::exception_ptr& eptr) -> result_t { std::rethrow_exception(eptr); },
                    []<similar_to<T> V>(V&& result) -> result_t { return std::forward<V>(result); }
                }, std::forward<U>(value).value_);
        }

    public:
        using detail::outcome_base<T>::outcome_base;

        template <typename U> friend decltype(auto) get(U&& value) { return get_impl(std::forward<U>(value)); }

        decltype(auto) get() & { return get_impl(*this); }
        decltype(auto) get() const & { return get_impl(*this); }
        decltype(auto) get() && { return get_impl(std::move(*this)); }
        decltype(auto) get() const && { return get_impl(std::move(*this)); }
        decltype(auto) operator*() & { return get_impl(*this); }
        decltype(auto) operator*() const & { return get_impl(*this); }
        decltype(auto) operator*() && { return get_impl(std::move(*this)); }
        decltype(auto) operator*() const && { return get_impl(std::move(*this)); }
    };

    template <>
    class outcome<void> final : public detail::outcome_base<void_tag_t>
    {
    public:
        outcome() = default;
        explicit(false) outcome(void_tag_t): outcome_base(void_tag) {}
        outcome(exceptional_outcome_t): outcome_base(exceptional_outcome) {}
        outcome(exceptional_outcome_t, const std::exception_ptr& eptr): outcome_base(exceptional_outcome, eptr) {}
        explicit outcome(std::in_place_t): outcome(void_tag) {}
        explicit outcome(std::in_place_t, void_tag_t): outcome(void_tag) {}

        friend void get(const outcome& value) { return value.get(); }

        void get() const
        {
            return std::visit(overload
                {
                    [](std::monostate) { throw bad_outcome_access(); },
                    [](const std::exception_ptr& eptr) { std::rethrow_exception(eptr); },
                    [](void_tag_t) {}
                }, value_);
        }

        void operator*() const { return get(); }
    };

    template <typename T>
    class outcome<T&> final : public detail::outcome_base<T*>
    {
    private:
        using base = detail::outcome_base<T*>;

    public:
        outcome() = default;
        explicit(false) outcome(T& value): base(std::addressof(value)) {}
        explicit(false) outcome(const T&& value) = delete;
        outcome(exceptional_outcome_t): base(exceptional_outcome) {}
        outcome(exceptional_outcome_t, const std::exception_ptr& eptr): base(exceptional_outcome, eptr) {}
        explicit outcome(std::in_place_t, T& value): outcome(value) {}
        explicit outcome(std::in_place_t, const T&& value) = delete;

        friend T& get(const outcome& value) { return value.get(); }

        T& get() const
        {
            return std::visit(overload
                {
                    [](std::monostate) -> T& { throw bad_outcome_access(); },
                    [](const std::exception_ptr& eptr) -> T& { std::rethrow_exception(eptr); },
                    [](T* ptr) -> T& { return *ptr; }
                }, this->value_);
        }

        T& operator*() const { return get(); }
    };

    template <typename T>
    class outcome<T&&> final : public detail::outcome_base<T*>
    {
    private:
        using base = detail::outcome_base<T*>;
        static T* addressof(T& ref) { return std::addressof(ref); }

    public:
        outcome() = default;
        explicit(false) outcome(T&& value): base(addressof(value)) {}
        outcome(exceptional_outcome_t): base(exceptional_outcome) {}
        outcome(exceptional_outcome_t, const std::exception_ptr& eptr): base(exceptional_outcome, eptr) {}
        explicit outcome(std::in_place_t, T&& value): outcome(value) {}

        friend T&& get(const outcome& value) { return value.get(); }

        T&& get() const
        {
            return std::visit(overload
                {
                    [](std::monostate) -> T&& { throw bad_outcome_access(); },
                    [](const std::exception_ptr& eptr) -> T& { std::rethrow_exception(eptr); },
                    [](T* ptr) -> T&& { return std::move(*ptr); }
                }, this->value_);
        }

        T&& operator*() const { return get(); }
    };

    template <typename T> requires (!std::same_as<T, std::in_place_t> && !std::same_as<T, exceptional_outcome_t>)
    outcome(T) -> outcome<T>;

    outcome(void_tag_t) -> outcome<void>;

    template <typename T, typename E>
    outcome<T> make_exceptional_outcome(E&& exception)
    {
        return
        {
            exceptional_outcome,
            std::make_exception_ptr(std::forward<E>(exception))
        };
    }
}
