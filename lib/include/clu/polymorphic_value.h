// Rough implementation of P0201R3: A polymorphic value-type for C++

#pragma once

#include <memory>
#include <typeinfo>

#include "concepts.h"

namespace clu
{
    // @formatter:off
    template <typename C, typename T>
    concept copier_of = std::copy_constructible<C>
        && requires(C copier, const T& value) { { copier(value) }->std::convertible_to<T*>; };

    template <typename D, typename T>
    concept deleter_of = std::copy_constructible<D>
        && requires(D deleter, T* ptr) { { deleter(ptr) } noexcept; };
    // @formatter:on

    template <typename T>
    struct default_copy
    {
        [[nodiscard]] T* operator()(const T& value) const { return new T(value); }
    };

    class bad_polymorphic_value_construction : public std::exception
    {
    public:
        bad_polymorphic_value_construction() noexcept {}
        const char* what() const noexcept override { return "bad polymorphic value construction"; }
    };

    namespace detail
    {
        template <typename T>
        struct control_block
        {
            virtual ~control_block() noexcept = default;
            virtual std::unique_ptr<control_block<T>> clone() const = 0;
            virtual T* get() = 0;
        };

        template <typename T, typename U>
        class inplace_control_block final : public control_block<T>
        {
        private:
            U value_;

        public:
            template <typename... Ts>
            explicit inplace_control_block(Ts&&... args): value_(std::forward<Ts>(args)...) {}

            ~inplace_control_block() noexcept override = default;
            std::unique_ptr<control_block<T>> clone() const override { return std::make_unique<inplace_control_block>(*this); }
            T* get() override { return static_cast<T*>(std::addressof(value_)); }
        };

        template <typename T, typename U, typename C, typename D>
        class pointer_control_block final : public control_block<T>
        {
        private:
            [[no_unique_address]] C copier_;
            std::unique_ptr<U, D> ptr_;

        public:
            pointer_control_block(U* ptr, C copier, D deleter):
                copier_(std::move(copier)), ptr_(ptr, std::move(deleter)) {}

            ~pointer_control_block() noexcept override = default;

            std::unique_ptr<control_block<T>> clone() const override
            {
                return std::make_unique<pointer_control_block>(
                    copier_(*ptr_), copier_, ptr_.get_deleter());
            }

            T* get() override { return static_cast<T*>(ptr_.get()); }
        };

        template <typename T, typename U>
        class delegating_control_block final : public control_block<T>
        {
        private:
            std::unique_ptr<control_block<U>> cb_;

        public:
            explicit delegating_control_block(std::unique_ptr<control_block<U>> cb): cb_(std::move(cb)) {}
            ~delegating_control_block() noexcept override = default;
            std::unique_ptr<control_block<T>> clone() const override { return std::make_unique<delegating_control_block>(cb_->clone()); }
            T* get() override { return static_cast<T*>(cb_->get()); }
        };
    }

    template <typename T>
    class polymorphic_value
    {
    public:
        using element_type = T;

    private:
        template <typename U> friend class polymorphic_value;
        std::unique_ptr<detail::control_block<T>> cb_;

    public:
        constexpr polymorphic_value() noexcept = default;

        template <typename U, copier_of<U> C = default_copy<U>, deleter_of<U> D = std::default_delete<U>>
            requires std::convertible_to<U*, T*>
        explicit polymorphic_value(U* ptr, C copier = C{}, D deleter = D{})
        {
            if constexpr (std::is_same_v<C, default_copy<U>> && std::is_same_v<D, std::default_delete<U>>)
                if (typeid(U) != typeid(*ptr))
                    throw bad_polymorphic_value_construction();
            cb_ = std::make_unique<detail::pointer_control_block<T, U, C, D>>(ptr, std::move(copier), std::move(deleter));
        }

        template <typename... Ts> requires std::copy_constructible<T>
        explicit polymorphic_value(std::in_place_t, Ts&&... args):
            cb_(std::make_unique<detail::inplace_control_block<T, T>>(std::forward<Ts>(args)...)) {}

        template <std::copy_constructible U, typename... Ts>
        explicit polymorphic_value(std::in_place_type_t<U>, Ts&&... args):
            cb_(std::make_unique<detail::inplace_control_block<T, U>>(std::forward<Ts>(args)...)) {}

        polymorphic_value(const polymorphic_value& other): cb_(other.cb_->clone()) {}

        template <typename U>
            requires std::convertible_to<U*, T*> && !std::same_as<T, U>
        explicit(false) polymorphic_value(const polymorphic_value<U>& other):
            cb_(std::make_unique<detail::delegating_control_block<T, U>>(other.cb_->clone())) {}

        polymorphic_value(polymorphic_value&&) noexcept = default;

        template <typename U>
            requires std::convertible_to<U*, T*> && !std::same_as<T, U>
        explicit(false) polymorphic_value(polymorphic_value<U>&& other):
            cb_(std::make_unique<detail::delegating_control_block<T, U>>(std::move(other.cb_))) {}

        template <typename U> requires
            !similar_to<U, polymorphic_value> &&
            std::copy_constructible<std::remove_cvref_t<U>> &&
            std::convertible_to<std::remove_cvref_t<U>*, T*>
        explicit(false) polymorphic_value(U&& value):
            cb_(std::make_unique<detail::inplace_control_block<T, U>>(std::forward<U>(value))) {}

        ~polymorphic_value() noexcept = default;

        polymorphic_value& operator=(const polymorphic_value& other)
        {
            if (&other != this) cb_ = other.cb_->clone();
            return *this;
        }

        template <typename U>
            requires std::convertible_to<U*, T*> && !std::same_as<T, U>
        polymorphic_value& operator=(const polymorphic_value<U>& other)
        {
            cb_ = std::make_unique<detail::delegating_control_block<T, U>>(other.cb_->clone());
            return *this;
        }

        polymorphic_value& operator=(polymorphic_value&&) noexcept = default;

        template <typename U>
            requires std::convertible_to<U*, T*> && !std::same_as<T, U>
        polymorphic_value& operator=(polymorphic_value<U>&& other)
        {
            cb_ = std::make_unique<detail::delegating_control_block<T, U>>(std::move(other.cb_));
            return *this;
        }

        template <typename U> requires
            !similar_to<U, polymorphic_value> &&
            std::copy_constructible<std::remove_cvref_t<U>> &&
            std::convertible_to<U*, T*>
        polymorphic_value& operator=(U&& value)
        {
            cb_ = std::make_unique<detail::inplace_control_block<T, U>>(std::forward<U>(value));
            return *this;
        }

        void swap(polymorphic_value& other) noexcept { std::swap(cb_, other.cb_); }

        [[nodiscard]] T& operator*() noexcept /* strengthened */ { return *cb_->get(); }
        [[nodiscard]] const T& operator*() const noexcept /* strengthened */ { return *cb_->get(); }
        [[nodiscard]] T* operator->() noexcept /* strengthened */ { return cb_->get(); }
        [[nodiscard]] const T* operator->() const noexcept /* strengthened */ { return cb_->get(); }
        [[nodiscard]] explicit operator bool() const noexcept { return cb_ != nullptr; }

        friend void swap(polymorphic_value& lhs, polymorphic_value& rhs) noexcept { lhs.swap(rhs); }
    };

    template <typename T, typename... Ts>
    [[nodiscard]] auto make_polymorphic_value(Ts&&... args)
    {
        return polymorphic_value<T>(
            std::in_place, std::forward<Ts>(args)...);
    }

    template <typename T, typename U, typename... Ts>
    [[nodiscard]] auto make_polymorphic_value(Ts&&... args)
    {
        return polymorphic_value<T>(
            std::in_place_type<U>, std::forward<Ts>(args)...);
    }
}
