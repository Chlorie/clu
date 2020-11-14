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
        template <typename T, typename Derived>
        class outcome_base
        {
        protected:
            std::variant<std::monostate, T, std::exception_ptr> value_;

            Derived& self() & { return static_cast<Derived&>(*this); }
            const Derived& self() const & { return static_cast<const Derived&>(*this); }
            Derived&& self() && { return std::move(static_cast<Derived&>(*this)); }
            const Derived&& self() const && { return std::move(static_cast<const Derived&>(*this)); }

        public:
            outcome_base() = default;
            explicit(false) outcome_base(const T& value): value_(value) {}
            explicit(false) outcome_base(T&& value): value_(std::move(value)) {}
            explicit(false) outcome_base(const std::exception_ptr& eptr): value_(eptr) {}
            template <typename E> outcome_base(exceptional_outcome_t, const E& exception): value_(std::make_exception_ptr(exception)) {}

            template <typename... Ts> requires std::constructible_from<T, Ts...>
            explicit outcome_base(std::in_place_t, Ts&&... args): value_(std::in_place_index<1>, std::forward<Ts>(args)...) {}

            Derived& operator=(const T& value) // NOLINT(misc-unconventional-assign-operator)
            {
                value_ = value;
                return self();
            }

            Derived& operator=(T&& value) // NOLINT(misc-unconventional-assign-operator)
            {
                value_ = std::move(value);
                return self();
            }

            Derived& operator=(const std::exception_ptr& eptr) // NOLINT(misc-unconventional-assign-operator)
            {
                value_ = eptr;
                return self();
            }

            template <typename E> void set_exception(const E& exception) { value_ = std::make_exception_ptr(exception); }

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

    template <typename T>
    class outcome final : public detail::outcome_base<T, outcome<T>>
    {
    private:
        using base = detail::outcome_base<T, outcome<T>>;

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
        using value_type = T;
        using base::base;
        using base::operator=;

        template <typename... Ts> requires std::constructible_from<T, Ts&&...>
        void emplace(Ts&&... args) { this->value_.template emplace<1>(std::forward<Ts>(args)...); }

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
    class outcome<void> final : public detail::outcome_base<void_tag_t, outcome<void>>
    {
    private:
        using base = outcome_base<void_tag_t, outcome<void>>;

    public:
        using value_type = void;

        outcome() = default;
        explicit(false) outcome(void_tag_t): outcome_base(void_tag) {}
        explicit(false) outcome(const std::exception_ptr& eptr): outcome_base(eptr) {}
        template <typename E> outcome(exceptional_outcome_t, const E& exception): outcome_base(exceptional_outcome, exception) {}
        explicit outcome(std::in_place_t): outcome(void_tag) {}
        explicit outcome(std::in_place_t, void_tag_t): outcome(void_tag) {}

        using base::operator=;

        void emplace() { this->value_.emplace<1>(); }
        void emplace(void_tag_t) { this->value_.emplace<1>(); }

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
    class outcome<T&> final : public detail::outcome_base<T*, outcome<T&>>
    {
    private:
        using base = detail::outcome_base<T*, outcome<T&>>;

    public:
        using value_type = T&;

        outcome() = default;
        explicit(false) outcome(T& value): base(std::addressof(value)) {}
        explicit(false) outcome(const T&& value) = delete;
        explicit(false) outcome(const std::exception_ptr& eptr): base(eptr) {}
        template <typename E> outcome(exceptional_outcome_t, const E& exception): base(exceptional_outcome, exception) {}
        explicit outcome(std::in_place_t, T& value): outcome(value) {}
        explicit outcome(std::in_place_t, const T&& value) = delete;

        using base::operator=;

        void emplace(T& value) { this->value_.template emplace<1>(std::addressof(value)); }
        void emplace(const T&& value) = delete;

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
    class outcome<T&&> final : public detail::outcome_base<T*, outcome<T&&>>
    {
    private:
        using base = detail::outcome_base<T*, outcome<T&&>>;
        static T* addressof(T& ref) { return std::addressof(ref); }

    public:
        using value_type = T&&;

        outcome() = default;
        explicit(false) outcome(T&& value): base(addressof(value)) {}
        explicit(false) outcome(const std::exception_ptr& eptr): base(eptr) {}
        template <typename E> outcome(exceptional_outcome_t, const E& exception): base(exceptional_outcome, exception) {}
        explicit outcome(std::in_place_t, T&& value): outcome(value) {}

        using base::operator=;

        void emplace(T&& value) { this->value_.template emplace<1>(addressof(value)); }

        T&& get() const
        {
            return std::visit(overload
                {
                    [](std::monostate) -> T&& { throw bad_outcome_access(); },
                    [](const std::exception_ptr& eptr) -> T&& { std::rethrow_exception(eptr); },
                    [](T* ptr) -> T&& { return std::move(*ptr); }
                }, this->value_);
        }

        T&& operator*() const { return get(); }
    };

    outcome(void_tag_t) -> outcome<void>;
    template <typename T> requires !same_as_any_of<T, std::in_place_t, exceptional_outcome_t> outcome(T) -> outcome<T>;
}
