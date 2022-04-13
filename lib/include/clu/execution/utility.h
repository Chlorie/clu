#pragma once

#include "execution_traits.h"
#include "../macros.h"

namespace clu::exec
{
    namespace detail
    {
        template <typename T, typename U>
            requires decays_to<T, T>
        constexpr copy_cvref_t<U&&, T> c_cast(U&& u)
        {
            CLU_GCC_WNO_OLD_STYLE_CAST
            // ReSharper disable once CppCStyleCast
            return (copy_cvref_t<U&&, T>)static_cast<U&&>(u);
            CLU_GCC_RESTORE_WARNING
        }

        struct no_base
        {
        };

        namespace recv_adpt
        {
            template <typename R, typename... Args>
            concept has_set_value = requires(R&& recv, Args&&... args)
            {
                static_cast<R&&>(recv).set_value(static_cast<Args&&>(args)...);
            };

            template <typename R, typename E>
            concept has_set_error = requires(R&& recv, E&& err)
            {
                static_cast<R&&>(recv).set_error(static_cast<E&&>(err));
            };

            template <typename R>
            concept has_set_stopped = requires(R&& recv)
            {
                static_cast<R&&>(recv).set_stopped();
            };

            template <typename R>
            concept has_get_env = requires(const R& recv)
            {
                recv.get_env();
            };

            // clang-format off
            template <typename R, typename B, typename... Args>
            concept inherits_set_value = requires { typename R::set_value; } && tag_invocable<set_value_t, B, Args...>;
            template <typename R, typename B, typename E>
            concept inherits_set_error = requires { typename R::set_error; } && tag_invocable<set_error_t, B, E>;
            template <typename R, typename B>
            concept inherits_set_stopped = requires { typename R::set_stopped; } && tag_invocable<set_stopped_t, B>;
            template <typename R, typename B>
            concept inherits_get_env = requires { typename R::get_env; } && tag_invocable<get_env_t, B>;
            // clang-format on

            template <typename Derived, typename Base>
            struct receiver_adaptor_
            {
                class type;
            };

            template <typename Derived, typename Base>
            class receiver_adaptor_<Derived, Base>::type
            {
            private:
                constexpr static bool has_base = !std::same_as<Base, no_base>;

            public:
                using set_value = void;
                using set_error = void;
                using set_stopped = void;
                using get_env = void;

                type() requires(std::same_as<Base, no_base>) || std::is_default_constructible_v<Base> = default;

                // clang-format off
                template <typename B> requires
                    (!std::same_as<Base, no_base>) &&
                    std::constructible_from<Base, B>
                explicit type(B&& base) noexcept(std::is_nothrow_constructible_v<Base, B>):
                    base_(static_cast<B&&>(base)) {}
                // clang-format on

                Base& base() & noexcept requires has_base { return base_; }
                const Base& base() const& noexcept requires has_base { return base_; }
                Base&& base() && noexcept requires has_base { return std::move(base_); }
                const Base&& base() const&& noexcept requires has_base { return std::move(base_); }

                // private:
                CLU_NO_UNIQUE_ADDRESS Base base_;

                template <typename D>
                constexpr static decltype(auto) get_base(D&& d) noexcept
                {
                    if constexpr (has_base)
                        return detail::c_cast<type>(static_cast<D&&>(d)).base();
                    else
                        return static_cast<D&&>(d).base();
                }

                template <typename D>
                using base_type = decltype(get_base(std::declval<D>()));

                // Not to spec:
                // Requiring std::derived_from<D, Derived> and make the parameter
                // reference to D for all the tag_invoke overloads to make gcc happy

                template <std::derived_from<Derived> D, typename... Args>
                    requires has_set_value<D, Args...> || inherits_set_value<D, base_type<D>, Args...>
                constexpr friend void tag_invoke(set_value_t, D&& self, Args&&... args) noexcept
                {
                    if constexpr (has_set_value<Derived, Args...>)
                    {
                        static_assert(noexcept(std::declval<Derived>().set_value(std::declval<Args>()...)),
                            "set_value should be noexcept");
                        static_cast<Derived&&>(self).set_value(static_cast<Args&&>(args)...);
                    }
                    else
                        exec::set_value(get_base(static_cast<Derived&&>(self)), static_cast<Args&&>(args)...);
                }

                template <typename E, std::derived_from<Derived> D>
                    requires has_set_error<D, E> || inherits_set_error<D, base_type<D>, E>
                constexpr friend void tag_invoke(set_error_t, D&& self, E&& err) noexcept
                {
                    if constexpr (has_set_error<Derived, E>)
                    {
                        static_assert(noexcept(std::declval<Derived>().set_error(std::declval<E>())),
                            "set_error should be noexcept");
                        static_cast<Derived&&>(self).set_error(static_cast<E&&>(err));
                    }
                    else
                        exec::set_error(get_base(static_cast<Derived&&>(self)), static_cast<E&&>(err));
                }

                template <std::derived_from<Derived> D>
                    requires has_set_stopped<D> || inherits_set_stopped<D, base_type<D>>
                constexpr friend void tag_invoke(set_stopped_t, D&& self) noexcept
                {
                    if constexpr (has_set_stopped<Derived>)
                    {
                        static_assert(
                            noexcept(std::declval<Derived>().set_stopped()), "set_stopped should be noexcept");
                        static_cast<Derived&&>(self).set_stopped();
                    }
                    else
                        exec::set_stopped(get_base(static_cast<Derived&&>(self)));
                }

                template <std::derived_from<Derived> D>
                    requires has_get_env<D> || inherits_get_env<D, base_type<const D&>>
                constexpr friend decltype(auto) tag_invoke(get_env_t, const D& self) noexcept
                {
                    if constexpr (has_get_env<Derived>)
                    {
                        static_assert(noexcept(self.get_env()), "get_env should be noexcept");
                        return self.get_env();
                    }
                    else
                        return exec::get_env(get_base(self));
                }

                template <recv_qry::fwd_recv_query Cpo, std::derived_from<Derived> D, typename... Args>
                    requires callable<Cpo, base_type<const D&>, Args...>
                constexpr friend decltype(auto) tag_invoke(Cpo cpo, const D& self, Args&&... args) noexcept(
                    nothrow_callable<Cpo, base_type<const D&>, Args...>)
                {
                    return cpo(get_base(self), static_cast<Args&&>(args)...);
                }
            };

            template <class_type Derived, typename Base = no_base>
            using receiver_adaptor = typename receiver_adaptor_<Derived, Base>::type;
        } // namespace recv_adpt

        namespace env_adpt
        {
            template <typename Env, typename Qry, typename T>
            struct env_t_
            {
                class type;
            };

            template <typename Env, typename Qry, typename T>
            using adapted_env_t = typename env_t_<std::remove_cvref_t<Env>, Qry, std::remove_cvref_t<T>>::type;

            template <typename Env, typename Qry, typename T>
            class env_t_<Env, Qry, T>::type
            {
            public:
                // clang-format off
                template <typename Env2, typename T2>
                constexpr type(Env2&& base, T2&& value) noexcept(
                    std::is_nothrow_constructible_v<Env, Env2> &&
                    std::is_nothrow_constructible_v<T, T2>):
                    base_(static_cast<Env2&&>(base)),
                    value_(static_cast<T2&&>(value)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS Env base_;
                CLU_NO_UNIQUE_ADDRESS T value_;

                // clang-format off
                template <typename Cpo, typename... Ts> requires
                    (!std::same_as<Cpo, Qry>) &&
                    tag_invocable<Cpo, const Env&, Ts...>
                constexpr friend auto tag_invoke(Cpo, const type& self, Ts&&... args)
                    noexcept(nothrow_tag_invocable<Cpo, const Env&, Ts...>)
                // clang-format on
                {
                    return clu::tag_invoke(Cpo{}, self.base_, static_cast<Ts&&>(args)...);
                }

                constexpr friend auto tag_invoke(Qry, const type& self) noexcept(
                    std::is_nothrow_copy_constructible_v<T>)
                {
                    return self.value_;
                }
            };

            template <typename Env, typename Qry, typename T>
            constexpr auto make_env(Env&& base, Qry, T&& value) noexcept(
                std::is_nothrow_constructible_v<adapted_env_t<Env, Qry, T>, Env, T>)
            {
                if constexpr (std::is_same_v<Env, no_env>)
                    return no_env{};
                else
                    return adapted_env_t<Env, Qry, T>(static_cast<Env&&>(base), static_cast<T&&>(value));
            }
        } // namespace env_adpt
    } // namespace detail

    using detail::recv_adpt::receiver_adaptor;
    using detail::env_adpt::make_env;
    using detail::env_adpt::adapted_env_t;

    template <class P>
        requires std::is_class_v<P> && std::same_as<P, std::remove_cvref_t<P>>
    class with_awaitable_senders
    {
    public:
        template <typename P2>
            requires(!std::same_as<P2, void>)
        void set_continuation(const coro::coroutine_handle<P2> h) noexcept
        {
            cont_ = h;
            if constexpr (requires(P2 & other) { other.unhandled_stopped(); })
                stopped_handler_ = [](void* p) noexcept -> coro::coroutine_handle<>
                { return coro::coroutine_handle<P2>::from_address(p).promise().unhandled_stopped(); };
            else
                stopped_handler_ = default_stopped_handler;
        }

        coro::coroutine_handle<> continuation() const noexcept { return cont_; }
        coro::coroutine_handle<> unhandled_stopped() const noexcept { return stopped_handler_(cont_.address()); }

        template <typename T>
        decltype(auto) await_transform(T&& value)
        {
            return exec::as_awaitable(static_cast<T&&>(value), static_cast<P&>(*this));
        }

    private:
        [[noreturn]] static coro::coroutine_handle<> default_stopped_handler(void*) noexcept { std::terminate(); }

        coro::coroutine_handle<> cont_{};
        coro::coroutine_handle<> (*stopped_handler_)(void*) noexcept = default_stopped_handler;
    };
} // namespace clu::exec
