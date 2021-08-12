// Rough implementation of P0323R10: std::expected
// Expected reference is also supported

#pragma once

#include <concepts>
#include <exception>
#include <utility>

#include "type_traits.h"
#include "scope.h"

namespace clu
{
    template <typename E> class unexpected;
    template <typename E> class bad_expected_access;

    struct unexpect_t
    {
        constexpr explicit unexpect_t() = default;
    };
    inline constexpr unexpect_t unexpect;

    namespace detail
    {
        template <typename T, typename U>
        concept allow_conversion =
        (!std::constructible_from<T, U&>) &&
        (!std::constructible_from<T, U>) &&
        (!std::constructible_from<T, const U&>) &&
        (!std::constructible_from<T, const U>) &&
        (!std::convertible_to<U&, T>) &&
        (!std::convertible_to<U, T>) &&
        (!std::convertible_to<const U&, T>) &&
        (!std::convertible_to<const U, T>);
    }

    template <typename E>
    class unexpected
    {
        static_assert(std::destructible<E> && std::is_object_v<E>,
            "unexpected type must satisfy Cpp17Destructible");
    private:
        E value_;

    public:
        constexpr unexpected(const unexpected&) = default;
        constexpr unexpected(unexpected&&) noexcept = default;
        constexpr unexpected& operator=(const unexpected&) = default;
        constexpr unexpected& operator=(unexpected&&) noexcept = default;

        template <typename Err> requires
            std::constructible_from<E, Err> &&
            (!std::same_as<std::remove_cvref_t<Err>, std::in_place_t>) &&
            (!std::same_as<std::remove_cvref_t<Err>, unexpected>)
        constexpr explicit unexpected(Err&& error): value_(static_cast<Err&>(error)) {}

        template <typename... Args>
            requires std::constructible_from<E, Args...>
        constexpr explicit unexpected(std::in_place_t, Args&&... args):
            value_(static_cast<Args&&>(args)...) {}

        template <typename U, typename... Args>
            requires std::constructible_from<E, std::initializer_list<U>&, Args...>
        constexpr explicit unexpected(std::in_place_t, std::initializer_list<U> ilist, Args&&... args):
            value_(ilist, static_cast<Args&&>(args)...) {}

        template <typename Err>
            requires std::constructible_from<E, const Err&> && detail::allow_conversion<E, unexpected<Err>>
        constexpr explicit(!std::convertible_to<const Err&, E>) unexpected(const unexpected<Err>& other):
            value_(other.value()) {}

        template <typename Err>
            requires std::constructible_from<E, Err> && detail::allow_conversion<E, unexpected<Err>>
        constexpr explicit(!std::convertible_to<Err, E>) unexpected(unexpected<Err>&& other):
            value_(std::move(other).value()) {}

        template <typename Err = E>
            requires std::assignable_from<E, const Err&>
        constexpr unexpected& operator=(const unexpected<Err>& other)
        {
            value_ = other.value();
            return *this;
        }

        template <typename Err = E>
            requires std::assignable_from<E, Err>
        constexpr unexpected& operator=(unexpected<Err>&& other)
        {
            value_ = std::move(other).value();
            return *this;
        }

        constexpr E& value() & noexcept { return value_; }
        constexpr const E& value() const & noexcept { return value_; }
        constexpr E&& value() && noexcept { return std::move(value_); }
        constexpr const E&& value() const && noexcept { return std::move(value_); }

        constexpr void swap(unexpected& other) noexcept(std::is_nothrow_swappable_v<E>)
            requires std::swappable<E> { std::ranges::swap(value_, other.value_); }

        template <class E1, class E2>
        [[nodiscard]] constexpr friend bool operator==(const unexpected<E1>& lhs, const unexpected<E2>& rhs)
        {
            return lhs.value() == rhs.value();
        }

        constexpr friend void swap(unexpected& lhs, unexpected& rhs) noexcept(std::is_nothrow_swappable_v<E>)
            requires std::swappable<E> { lhs.swap(rhs); }
    };

    template <typename E> unexpected(E) -> unexpected<E>;

    template <>
    class bad_expected_access<void> : public std::exception
    {
    public:
        explicit bad_expected_access() = default;
    };

    template <typename E>
    class bad_expected_access : public bad_expected_access<void>
    {
    private:
        E value_;
    public:
        explicit bad_expected_access(E value): value_(std::move(value)) {}
        const char* what() const override { return "bad expected access"; }
        [[nodiscard]] E& error() & noexcept { return value_; }
        [[nodiscard]] const E& error() const & noexcept { return value_; }
        [[nodiscard]] E&& error() && noexcept { return std::move(value_); }
        [[nodiscard]] const E&& error() const && noexcept { return std::move(value_); }
    };

    template <typename T, typename E>
    class expected
    {
    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;
        template <typename U> using rebind = expected<U, error_type>;

        using pointer = std::remove_reference_t<T>*;
        using const_pointer = std::add_const_t<std::remove_reference_t<T>>*;

    private:
        using real_value = conditional_t<std::is_void_v<T>, monostate,
            conditional_t<std::is_lvalue_reference_v<T>, std::remove_reference_t<T>*, T>>;

        template <typename U, typename G>
        static constexpr bool is_explicit =
            (!std::is_void_v<T> && !std::is_void_v<U> && !std::is_convertible_v<const U&, T>) ||
            !std::is_convertible_v<const G&, E>;

        bool engaged_ = false;
        union data_t
        {
            monostate dummy_;
            real_value value_;
            unexpected_type err_;

            constexpr data_t() noexcept : dummy_{} {}

            constexpr data_t(const data_t&) noexcept requires
                std::is_trivially_copy_constructible_v<real_value> &&
                std::is_trivially_copy_constructible_v<unexpected_type> = default;
            constexpr data_t(const data_t&) requires(
                !std::is_copy_constructible_v<real_value> ||
                !std::is_copy_constructible_v<unexpected_type>) = delete;
            constexpr data_t(const data_t&) noexcept: dummy_{} {}

            constexpr data_t(data_t&&) noexcept requires
                std::is_trivially_move_constructible_v<real_value> &&
                std::is_trivially_move_constructible_v<unexpected_type> = default;
            constexpr data_t(data_t&&) requires(
                !std::is_move_constructible_v<real_value> ||
                !std::is_move_constructible_v<unexpected_type>) = delete;
            constexpr data_t(data_t&&) noexcept: dummy_{} {}

            constexpr data_t& operator=(const data_t&) noexcept requires
                std::is_trivially_copy_assignable_v<real_value> &&
                std::is_trivially_copy_assignable_v<unexpected_type> = default;
            constexpr data_t& operator=(const data_t&) requires(
                !std::is_copy_assignable_v<real_value> ||
                !std::is_copy_assignable_v<unexpected_type>) = delete;
            constexpr data_t& operator=(const data_t&) noexcept { return *this; }

            constexpr data_t& operator=(data_t&&) noexcept requires
                std::is_trivially_move_assignable_v<real_value> &&
                std::is_trivially_move_assignable_v<unexpected_type> = default;
            constexpr data_t& operator=(data_t&&) requires(
                !std::is_move_assignable_v<real_value> ||
                !std::is_move_assignable_v<unexpected_type>) = delete;
            constexpr data_t& operator=(data_t&&) noexcept { return *this; }

            constexpr ~data_t() noexcept requires
                std::is_trivially_destructible_v<real_value> &&
                std::is_trivially_destructible_v<unexpected_type> = default;
            constexpr ~data_t() noexcept {}
        } data_;

        template <typename Self>
        constexpr static decltype(auto) value_impl(Self&& self)
        {
            if (!self.engaged_)
                throw bad_expected_access<E>(static_cast<copy_cvref_t<Self&&, E>>(self.data_.err_));
            if constexpr (!std::is_void_v<T>)
                return *static_cast<Self&&>(self);
        }

    public:
        constexpr expected()
            requires std::default_initializable<T> || std::is_void_v<T> :
            engaged_(true) { new(std::addressof(data_.value_)) T(); }

        constexpr expected(const expected&) noexcept requires
            std::is_trivially_copy_constructible_v<real_value> &&
            std::is_trivially_copy_constructible_v<unexpected_type> = default;

        constexpr expected(const expected&) requires(
            !std::is_copy_constructible_v<real_value> ||
            !std::is_copy_constructible_v<unexpected_type>) = delete;

        constexpr expected(const expected& other)
        {
            if (other.engaged_)
            {
                new(std::addressof(data_.value_)) real_value(other.data_.value_);
                engaged_ = true;
            }
            else
                new(std::addressof(data_.err_)) unexpected<E>(other.error());
        }

        constexpr expected(expected&&) noexcept requires
            std::is_trivially_move_constructible_v<real_value> &&
            std::is_trivially_move_constructible_v<unexpected_type> = default;

        constexpr expected(expected&&) requires(
            !std::is_move_constructible_v<real_value> ||
            !std::is_move_constructible_v<unexpected_type>) = delete;

        constexpr expected(expected&& other) noexcept(
            std::is_nothrow_move_constructible_v<real_value> &&
            std::is_nothrow_move_constructible_v<unexpected_type>)
        {
            if (other.engaged_)
            {
                new(std::addressof(data_.value_)) real_value(std::move(other.data_.value_));
                engaged_ = true;
            }
            else
                new(std::addressof(data_.err_)) unexpected<E>(std::move(other.error()));
        }

        template <typename U, typename G> requires
            std::constructible_from<T, const U&> &&
            detail::allow_conversion<T, expected<U, G>> &&
            std::constructible_from<E, const G&> &&
            detail::allow_conversion<unexpected<E>, expected<U, G>>
        constexpr explicit(is_explicit<U, G>) expected(const expected<U, G>& other)
        {
            if (other.engaged_)
            {
                new(std::addressof(data_.value_)) real_value(other.data_.value_);
                engaged_ = true;
            }
            else
                new(std::addressof(data_.err_)) unexpected<E>(other.error());
        }

        template <typename U, typename G> requires
            std::constructible_from<T, U&&> &&
            detail::allow_conversion<T, expected<U, G>> &&
            std::constructible_from<E, G&&> &&
            detail::allow_conversion<unexpected<E>, expected<U, G>>
        constexpr explicit(is_explicit<U, G>) expected(expected<U, G>&& other)
        {
            if (other.engaged_)
            {
                new(std::addressof(data_.value_)) real_value(std::move(other.data_.value_));
                engaged_ = true;
            }
            else
                new(std::addressof(data_.err_)) unexpected<E>(std::move(other.error()));
        }

        template <typename U = T> requires
            (!std::is_void_v<T>) &&
            std::constructible_from<T, U&&> &&
            (!std::same_as<std::remove_cvref_t<U>, std::in_place_t>) &&
            (!std::same_as<std::remove_cvref_t<U>, expected<T, E>>) &&
            (!std::same_as<std::remove_cvref_t<U>, unexpected<E>>)
        constexpr explicit(!std::is_convertible_v<U&&, T>) expected(U&& value)
        {
            if constexpr (std::is_lvalue_reference_v<T>)
                new(std::addressof(data_.value_)) real_value(std::addressof(value));
            else
                new(std::addressof(data_.value_)) real_value(std::forward<U>(value));
            engaged_ = true;
        }

        template <typename G = E>
            requires std::constructible_from<E, const G&>
        constexpr explicit(!std::is_convertible_v<const G&, E>) expected(const unexpected<G>& err)
        {
            new(std::addressof(data_.err_)) unexpected_type(err);
        }

        template <typename G = E>
            requires std::constructible_from<E, G&&>
        constexpr explicit(!std::is_convertible_v<G&&, E>) expected(unexpected<G>&& err)
        {
            new(std::addressof(data_.err_)) unexpected_type(std::move(err));
        }

        template <typename... Args> requires
            (std::is_void_v<T> && sizeof...(Args) == 0) ||
            std::constructible_from<T, Args...>
        constexpr explicit expected(std::in_place_t, Args&&... args)
        {
            if constexpr (std::is_lvalue_reference_v<T>)
                new(std::addressof(data_.value_)) real_value(std::addressof(args)...);
            else
                new(std::addressof(data_.value_)) real_value(std::forward<Args>(args)...);
            engaged_ = true;
        }

        template <typename U, typename... Args>
            requires std::constructible_from<T, std::initializer_list<U>, Args...>
        constexpr explicit expected(std::in_place_t, const std::initializer_list<U> ilist, Args&&... args)
        {
            new(std::addressof(data_.value_)) real_value(ilist, std::forward<Args>(args)...);
            engaged_ = true;
        }

        template <typename... Args>
            requires std::constructible_from<E, Args...>
        constexpr explicit expected(unexpect_t, Args&&... args)
        {
            new(std::addressof(data_.err_)) unexpected_type(std::forward<Args>(args)...);
        }

        template <typename U, typename... Args>
            requires std::constructible_from<E, std::initializer_list<U>, Args...>
        constexpr explicit expected(unexpect_t, const std::initializer_list<U> ilist, Args&&... args)
        {
            new(std::addressof(data_.err_)) unexpected_type(ilist, std::forward<Args>(args)...);
        }

        constexpr ~expected() noexcept requires
            std::is_trivially_destructible_v<real_value> &&
            std::is_trivially_destructible_v<unexpected_type> = default;

        constexpr ~expected() noexcept
        {
            if (engaged_)
                data_.value_.~real_value();
            else
                data_.err_.~unexpected<E>();
        }

        constexpr expected& operator=(const expected&) noexcept requires
            std::is_trivially_copy_assignable_v<real_value> &&
            std::is_trivially_copy_assignable_v<unexpected_type> = default;

        constexpr expected& operator=(const expected&) requires
            (!std::copyable<real_value>) ||
            (!std::copyable<unexpected_type>) || (
                !std::is_nothrow_move_constructible_v<real_value> &&
                !std::is_nothrow_move_constructible_v<unexpected_type>) = delete;

        constexpr expected& operator=(const expected& other)
        {
            if (&other == this) return *this;
            if (engaged_)
            {
                if (other.engaged_)
                    data_.value_ = *other;
                else if constexpr (std::is_nothrow_copy_assignable_v<E>)
                {
                    data_.value_.~real_value();
                    new(std::addressof(data_.err_)) unexpected<E>(other.error());
                    engaged_ = false;
                }
                else if constexpr (std::is_nothrow_move_constructible_v<E>)
                {
                    unexpected<E> err(other.error());
                    data_.value_.~real_value();
                    new(std::addressof(data_.err_)) unexpected<E>(std::move(err));
                    engaged_ = false;
                }
                else
                {
                    real_value val(data_.value_);
                    data_.value_.~real_value();
                    scope_fail guard([&] { new(std::addressof(data_.value_)) real_value(std::move(val)); });
                    new(std::addressof(data_.err_)) unexpected<E>(other.error());
                    engaged_ = false;
                }
            }
            else
            {
                if (!other.engaged_)
                    data_.err_ = other.error();
                else if constexpr (std::is_nothrow_copy_assignable_v<T>)
                {
                    data_.err_.~unexpected<E>();
                    new(std::addressof(data_.value_)) real_value(other.data_.value_);
                    engaged_ = true;
                }
                else if constexpr (std::is_nothrow_move_constructible_v<T>)
                {
                    real_value val(other.data_.value_);
                    data_.err_.~unexpected<E>();
                    new(std::addressof(data_.value_)) real_value(std::move(val));
                    engaged_ = true;
                }
                else
                {
                    unexpected<E> err(data_.err_);
                    data_.err_.~unexpected<E>();
                    scope_fail guard([&] { new(std::addressof(data_.err_)) unexpected<E>(std::move(err)); });
                    new(std::addressof(data_.value_)) real_value(other.data_.value_);
                    engaged_ = true;
                }
            }
            return *this;
        }

        constexpr expected& operator=(expected&&) noexcept requires
            std::is_trivially_move_assignable_v<real_value> &&
            std::is_trivially_move_assignable_v<unexpected_type> = default;

        constexpr expected& operator=(expected&&) requires(
            !std::movable<real_value> ||
            !std::is_nothrow_move_constructible_v<unexpected_type> ||
            !std::is_nothrow_move_assignable_v<unexpected_type>) = delete;

        constexpr expected& operator=(expected&& other) noexcept(
            std::is_nothrow_move_constructible_v<real_value> &&
            std::is_nothrow_move_assignable_v<real_value>)
        {
            if (&other == this) return *this;
            if (engaged_)
            {
                if (other.engaged_)
                    data_.value_ = std::move(*other);
                else if constexpr (std::is_nothrow_move_constructible_v<E>)
                {
                    data_.value_.~real_value();
                    new(std::addressof(data_.err_)) unexpected<E>(std::move(other.error()));
                    engaged_ = false;
                }
                else
                {
                    real_value val(std::move(data_.value_));
                    data_.value_.~real_value();
                    scope_fail guard([&] { new(std::addressof(data_.value_)) real_value(std::move(val)); });
                    new(std::addressof(data_.err_)) unexpected<E>(std::move(other.error()));
                    engaged_ = false;
                }
            }
            else
            {
                if (!other.engaged_)
                    data_.err_ = std::move(other.error());
                else if constexpr (std::is_nothrow_move_constructible_v<T>)
                {
                    data_.err_.~unexpected<E>();
                    new(std::addressof(data_.value_)) real_value(std::move(other.data_.value_));
                    engaged_ = true;
                }
                else
                {
                    unexpected<E> err(std::move(data_.err_));
                    data_.err_.~unexpected<E>();
                    scope_fail guard([&] { new(std::addressof(data_.err_)) unexpected<E>(std::move(err)); });
                    new(std::addressof(data_.value_)) real_value(std::move(other.data_.value_));
                    engaged_ = true;
                }
            }
            return *this;
        }

        template <typename U = T> requires
            std::is_object_v<T> &&
            std::constructible_from<T, U> &&
            std::assignable_from<T&, U> &&
            (!std::same_as<expected, std::remove_cvref_t<U>>) &&
            std::is_nothrow_move_constructible_v<E>
        constexpr expected& operator=(U&& other)
        {
            if (engaged_)
                data_.value_ = static_cast<U&&>(other);
            else if constexpr (std::is_nothrow_constructible_v<T, U>)
            {
                data_.err_.~unexpected<E>();
                new(std::addressof(data_.value_)) real_value(static_cast<U&&>(other));
                engaged_ = true;
            }
            else
            {
                unexpected<E> err(std::move(data_.err_));
                data_.err_.~unexpected<E>();
                scope_fail guard([&] { new(std::addressof(data_.err_)) unexpected<E>(std::move(err)); });
                new(std::addressof(data_.value_)) real_value(static_cast<U&&>(other));
            }
            return *this;
        }

        template <typename U = T> requires
            std::is_lvalue_reference_v<T> &&
            std::is_lvalue_reference_v<U> &&
            std::constructible_from<T, U>
        constexpr expected& operator=(U&& ref) noexcept
        {
            if (engaged_)
                data_.value_ = std::addressof(ref);
            else
            {
                data_.err_.~unexpected_type();
                new(std::addressof(data_.value_)) real_value(std::addressof(ref));
                engaged_ = true;
            }
            return *this;
        }

        template <typename G = E> requires
            std::is_nothrow_copy_constructible_v<E> &&
            std::is_copy_assignable_v<E>
        constexpr expected& operator=(const unexpected<G>& err)
        {
            if (engaged_)
            {
                data_.value_.~real_value();
                new(std::addressof(data_.err_)) unexpected_type(err);
                engaged_ = false;
            }
            else
                data_.err_ = err;
            return *this;
        }

        template <typename G = E> requires
            std::is_nothrow_move_constructible_v<E> &&
            std::is_move_assignable_v<E>
        constexpr expected& operator=(unexpected<G>&& err)
        {
            if (engaged_)
            {
                data_.value_.~real_value();
                new(std::addressof(data_.err_)) unexpected_type(std::move(err));
                engaged_ = false;
            }
            else
                data_.err_ = std::move(err);
            return *this;
        }

        [[nodiscard]] constexpr pointer operator->() noexcept
            requires (!std::is_void_v<T>) { return std::addressof(data_.value_); }
        [[nodiscard]] constexpr const_pointer operator->() const noexcept
            requires (!std::is_void_v<T>) { return std::addressof(data_.value_); }

        [[nodiscard]] constexpr T& operator*() & noexcept
            requires std::is_object_v<T> { return data_.value_; }
        [[nodiscard]] constexpr const T& operator*() const & noexcept
            requires std::is_object_v<T> { return data_.value_; }
        [[nodiscard]] constexpr T&& operator*() && noexcept
            requires std::is_object_v<T> { return std::move(data_.value_); }
        [[nodiscard]] constexpr const T&& operator*() const && noexcept
            requires std::is_object_v<T> { return std::move(data_.value_); }
        [[nodiscard]] constexpr T operator*() const noexcept
            requires std::is_lvalue_reference_v<T> { return *data_.value_; }

        [[nodiscard]] constexpr T& value() &
            requires std::is_object_v<T> { return value_impl(*this); }
        [[nodiscard]] constexpr const T& value() const &
            requires std::is_object_v<T> { return value_impl(*this); }
        [[nodiscard]] constexpr T&& value() &&
            requires std::is_object_v<T> { return value_impl(std::move(*this)); }
        [[nodiscard]] constexpr const T&& value() const &&
            requires std::is_object_v<T> { return value_impl(std::move(*this)); }
        [[nodiscard]] constexpr T value() const &
            requires std::is_lvalue_reference_v<T> { return value_impl(*this); }
        [[nodiscard]] constexpr T value() &&
            requires std::is_lvalue_reference_v<T> { return value_impl(*this); }

        [[nodiscard]] constexpr E& error() & noexcept { return data_.err_.value(); }
        [[nodiscard]] constexpr const E& error() const & noexcept { return data_.err_.value(); }
        [[nodiscard]] constexpr E&& error() && noexcept { return std::move(data_.err_).value(); }
        [[nodiscard]] constexpr const E&& error() const && noexcept { return std::move(data_.err_).value(); }

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return engaged_; }
        [[nodiscard]] constexpr bool has_value() const noexcept { return engaged_; }

        constexpr void emplace() requires std::is_void_v<T>
        {
            if (!engaged_)
            {
                data_.err_.~unexpected_type();
                new(std::addressof(data_.value_)) real_value();
            }
        }

        template <typename... Args>
            requires std::is_nothrow_constructible_v<T, Args...>
        constexpr T& emplace(Args&&... args)
        {
            if constexpr (std::is_lvalue_reference_v<T>)
            {
                if (engaged_)
                    data_.value_ = real_value(std::addressof(args)...);
                else
                {
                    data_.err_.~unexpected_type();
                    new(std::addressof(data_.value_)) real_value(std::addressof(args)...);
                    engaged_ = true;
                }
                return *data_.value_;
            }
            else
            {
                if (engaged_)
                    data_.value_ = real_value(static_cast<Args&&>(args)...);
                else if constexpr (std::is_nothrow_constructible_v<T, Args...>)
                {
                    data_.err_.~unexpected_type();
                    new(std::addressof(data_.value_)) real_value(static_cast<Args&&>(args)...);
                    engaged_ = true;
                }
                else if constexpr (std::is_nothrow_move_constructible_v<T>)
                {
                    T value(static_cast<Args&&>(args)...);
                    data_.err_.~unexpected_type();
                    new(std::addressof(data_.value_)) real_value(std::move(value));
                    engaged_ = true;
                }
                else
                {
                    unexpected<E> err(std::move(data_.err_));
                    data_.err_.~unexpected_type();
                    scope_fail guard([&] { new(std::addressof(data_.err_)) unexpected<E>(std::move(err)); });
                    new(std::addressof(data_.value_)) real_value(static_cast<Args&&>(args)...);
                    engaged_ = true;
                }
                return data_.value_;
            }
        }

        template <typename U, typename... Args>
            requires std::is_nothrow_constructible_v<T, std::initializer_list<U>, Args...>
        constexpr T& emplace(const std::initializer_list<U> ilist, Args&&... args)
        {
            if (engaged_)
                data_.value_ = real_value(ilist, static_cast<Args&&>(args)...);
            else if constexpr (std::is_nothrow_constructible_v<T, Args...>)
            {
                data_.err_.~unexpected_type();
                new(std::addressof(data_.value_)) real_value(ilist, static_cast<Args&&>(args)...);
                engaged_ = true;
            }
            else if constexpr (std::is_nothrow_move_constructible_v<T>)
            {
                T value(ilist, static_cast<Args&&>(args)...);
                data_.err_.~unexpected_type();
                new(std::addressof(data_.value_)) real_value(std::move(value));
                engaged_ = true;
            }
            else
            {
                unexpected<E> err(std::move(data_.err_));
                data_.err_.~unexpected_type();
                scope_fail guard([&] { new(std::addressof(data_.err_)) unexpected<E>(std::move(err)); });
                new(std::addressof(data_.value_)) real_value(ilist, static_cast<Args&&>(args)...);
                engaged_ = true;
            }
            return data_.value_;
        }

        template <typename U>
            requires std::convertible_to<U, T>
        [[nodiscard]] constexpr T value_or(U&& value) const &
        {
            if (engaged_)
                return data_.value_;
            else
                return static_cast<T>(static_cast<U&&>(value));
        }

        template <typename U>
            requires std::convertible_to<U, T>
        [[nodiscard]] constexpr T value_or(U&& value) &&
        {
            if (engaged_)
                return std::move(data_.value_);
            else
                return static_cast<T>(static_cast<U&&>(value));
        }

        template <typename U, typename G>
            requires std::equality_comparable_with<T, U> && std::equality_comparable_with<E, G>
        [[nodiscard]] constexpr bool operator==(const expected<U, G>& other) const
        {
            if (engaged_ != other.has_value()) return false;
            return engaged_ ? **this == *other : error() == other.error();
        }

        template <typename U> requires std::equality_comparable_with<T, U>
        [[nodiscard]] constexpr bool operator==(const U& other) const { return engaged_ ? **this == other : false; }

        template <typename G> requires std::equality_comparable_with<E, G>
        [[nodiscard]] constexpr bool operator==(const unexpected<G>& other) const { return engaged_ ? false : data_.err_ == other; }

        constexpr void swap(expected& other) noexcept(
            std::is_nothrow_move_constructible_v<T> &&
            std::is_nothrow_swappable_v<T> &&
            std::is_nothrow_move_constructible_v<E> &&
            std::is_nothrow_swappable_v<E>) requires
            std::swappable<real_value> &&
            std::swappable<E> && (
                std::is_nothrow_move_constructible_v<real_value> ||
                std::is_nothrow_move_constructible_v<E>)
        {
            if (!engaged_)
            {
                if (other.engaged_)
                    other.swap(*this);
                else
                    std::ranges::swap(data_.err_, other.data_.err_);
            }
            else
            {
                if (other.engaged_)
                    std::ranges::swap(data_.value_, other.data_.value_);
                else if constexpr (std::is_nothrow_move_constructible_v<E>)
                {
                    unexpected<E> err(std::move(other.error()));
                    other.data_.err_.~unexcepted_type();
                    scope_fail guard([&] { new(std::addressof(other.data_.err_)) unexpected<E>(std::move(err)); });
                    new(std::addressof(other.data_.value_)) real_value(std::move(**this));
                    data_.value_.~real_value();
                    new(std::addressof(data_.err_)) unexpected<E>(std::move(err));
                    engaged_ = false;
                    other.engaged_ = true;
                }
                else if constexpr (std::is_nothrow_move_constructible_v<real_value>)
                {
                    real_value val(std::move(*other));
                    other.data_.value_.~real_value();
                    scope_fail guard([&] { new(std::addressof(other.data_.value_)) real_value(std::move(val)); });
                    new(std::addressof(data_.err_)) unexpected<E>(std::move(other.error()));
                    other.data_.err_.~unexpected<E>();
                    new(std::addressof(other.data_.value_)) unexpected<E>(std::move(val));
                    engaged_ = false;
                    other.engaged_ = true;
                }
            }
        }

        constexpr friend void swap(expected& lhs, expected& rhs) noexcept(noexcept(lhs.swap(rhs)))
            requires
            std::swappable<real_value> &&
            std::swappable<E> && (
                std::is_nothrow_move_constructible_v<real_value> ||
                std::is_nothrow_move_constructible_v<E>) { lhs.swap(rhs); }
    };
}
