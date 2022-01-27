#pragma once

#include "execution_traits.h"
#include "../meta_algorithm.h"
#include "../macros.h"

namespace clu::exec
{
    namespace detail
    {
        struct no_base {};

        template <typename T, typename U>
            requires decays_to<T, T>
        copy_cvref_t<U&&, T> c_cast(U&& u) noexcept
        {
            CLU_GCC_WNO_OLD_STYLE_CAST
            // ReSharper disable once CppCStyleCast
            return (copy_cvref_t<U&&, T>)static_cast<U&&>(u);
            CLU_GCC_RESTORE_WARNING
        }

        template <typename Derived, typename Base>
        struct receiver_adaptor_
        {
            class type;
        };

        // @formatter:off
        template <typename R, typename... Args>
        concept has_set_value = requires(R&& recv, Args&&... args)
        {
            static_cast<R&&>(recv).set_value(static_cast<Args&&>(args)...);
        };

        template <typename R, typename B, typename... Args>
        concept inherits_set_value = 
            requires { typename R::set_value; } &&
            receiver_of<B, Args...>;
            
        template <typename R, typename E>
        concept has_set_error = requires(R&& recv, E&& err)
        {
            static_cast<R&&>(recv).set_error(static_cast<E&&>(err));
        };

        template <typename R, typename B, typename E>
        concept inherits_set_error =
            requires { typename R::set_error; } &&
            receiver<B, E>;

        template <typename R>
        concept has_set_stopped = requires(R&& recv)
        {
            static_cast<R&&>(recv).set_stopped();
        };

        template <typename R>
        concept inherits_set_stopped = requires { typename R::set_stopped; };
        // @formatter:on

        template <typename Derived, typename Base>
        class receiver_adaptor_<Derived, Base>::type
        {
        private:
            static constexpr bool has_base = !std::same_as<Base, no_base>;

        public:
            using set_value = void;
            using set_error = void;
            using set_stopped = void;

            constexpr type() = default;

            template <typename B> requires
                (!std::same_as<Base, no_base>) &&
                std::constructible_from<Base, B>
            explicit type(B&& base): base_(static_cast<B&&>(base)) {}

            Base& base() & noexcept requires has_base { return base_; }
            const Base& base() const & noexcept requires has_base { return base_; }
            Base&& base() && noexcept requires has_base { return std::move(base_); }
            const Base&& base() const && noexcept requires has_base { return std::move(base_); }

        private:
            [[no_unique_address]] Base base_;

            template <typename D>
            using no_base_base_type = decltype(std::declval<D>().base());

            template <typename D>
            using base_type = meta::invoke<conditional_t<
                has_base,
                meta::compose_q<
                    meta::bind_back_q1<meta::quote2<copy_cvref>, Base>,
                    meta::_t_q
                >,
                meta::quote1<no_base_base_type>
            >, D>;

            template <typename D>
            static base_type<D> get_base(D&& d)
            {
                if constexpr (has_base)
                    return detail::c_cast<type>(static_cast<D&&>(d)).base();
                else
                    return static_cast<D&&>(d).base();
            }

            template <typename D = Derived, typename... Args>
            constexpr static bool set_val_noex()
            {
                if constexpr (has_set_value<D, Args...>)
                    return noexcept(std::declval<D>().set_value(std::declval<Args>()...));
                else
                    return noexcept(exec::set_value(std::declval<base_type<D>>(), std::declval<Args>()...));
            }

            template <typename D = Derived, typename... Args>
                requires has_set_value<D, Args...> || inherits_set_value<D, base_type<D>, Args...>
            constexpr friend void tag_invoke(set_value_t, Derived&& self, Args&&... args) noexcept(set_val_noex<D, Args...>())
            {
                if constexpr (has_set_value<D, Args...>)
                    static_cast<Derived&&>(self).set_value(static_cast<Args&&>(args)...);
                else
                    exec::set_value(get_base(static_cast<Derived&&>(self)), static_cast<Args&&>(args)...);
            }

            template <typename E, typename D = Derived>
                requires has_set_error<D, E> || inherits_set_error<D, base_type<D>, E>
            constexpr friend void tag_invoke(set_error_t, Derived&& self, E&& err) noexcept
            {
                if constexpr (has_set_error<D, E>)
                {
                    static_assert(noexcept(std::declval<Derived>().set_error(std::declval<E>())),
                        "set_error should be noexcept");
                    static_cast<Derived&&>(self).set_error(static_cast<E&&>(err));
                }
                else
                    exec::set_error(get_base(static_cast<Derived&&>(self)), static_cast<E&&>(err));
            }

            template <typename D = Derived>
                requires has_set_stopped<D> || inherits_set_stopped<D>
            constexpr friend void tag_invoke(set_stopped_t, Derived&& self) noexcept
            {
                if constexpr (has_set_stopped<D>)
                {
                    static_assert(noexcept(std::declval<Derived>().set_stopped()),
                        "set_stopped should be noexcept");
                    static_cast<Derived&&>(self).set_stopped();
                }
                else
                    exec::set_stopped(get_base(static_cast<Derived&&>(self)));
            }

            // @formatter:off
            template <fwd_recv_query Cpo, typename D = Derived, typename... Args>
                requires callable<Cpo, base_type<const D&>, Args...>
            constexpr friend auto tag_invoke(Cpo cpo, const Derived& self, Args&&... args)
                noexcept(nothrow_callable<Cpo, base_type<const D&>, Args...>)
                -> call_result_t<Cpo, base_type<const D&>, Args...>
            {
                return static_cast<Cpo&&>(cpo)(
                    get_base(self), static_cast<Args&&>(args)...);
            }
            // @formatter:on
        };
    }

    template <class_type Derived, typename Base = detail::no_base>
    using receiver_adaptor = typename detail::receiver_adaptor_<Derived, Base>::type;

    template <class P> requires
        std::is_class_v<P> &&
        std::same_as<P, std::remove_cvref_t<P>>
    class with_awaitable_senders
    {
    public:
        template <typename P2>
            requires (!std::same_as<P2, void>)
        void set_continuation(const coro::coroutine_handle<P2> h) noexcept
        {
            cont_ = h;
            if constexpr (requires(P2& other) { other.unhandled_stopped(); })
                done_handler_ = [](void* p) noexcept -> coro::coroutine_handle<>
                {
                    return coro::coroutine_handle<P2>::from_address(p)
                          .promise().unhandled_stopped();
                };
            else
                done_handler_ = default_stopped_handler;
        }

        coro::coroutine_handle<> continuation() const noexcept { return cont_; }
        coro::coroutine_handle<> unhandled_stopped() const noexcept { return done_handler_(cont_.address()); }

        template <typename T>
        decltype(auto) await_transform(T&& value)
        {
            return exec::as_awaitable(
                static_cast<T&&>(value), static_cast<P&>(*this));
        }

    private:
        [[noreturn]] static coro::coroutine_handle<> default_stopped_handler(void*) noexcept { std::terminate(); }

        coro::coroutine_handle<> cont_{};
        coro::coroutine_handle<> (*done_handler_)(void*) noexcept = default_stopped_handler;
    };
}

#include "../undef_macros.h"
