#pragma once

#include "execution_traits.h"
#include "utility.h"

namespace clu::exec
{
    namespace detail
    {
        // exec::just
        template <typename... Ts>
        struct just_snd_
        {
            struct type;

            template <typename R>
            struct ops_
            {
                struct type;
            };

            template <typename R>
            using ops = typename ops_<R>::type;
        };

        template <typename... Ts>
        using just_snd = typename just_snd_<Ts...>::type;

        template <typename... Ts>
        struct just_snd_<Ts...>::type :
            completion_signatures<set_value_t(Ts ...)>
        {
            using tuple_t = std::tuple<Ts...>;
            tuple_t values;

            template <forwarding<type> Self, receiver_of<Ts...> R>
                requires std::constructible_from<tuple_t, copy_cvref_t<Self, tuple_t>>
            friend auto tag_invoke(connect_t, Self&& snd, R&& recv)
            {
                return ops<std::remove_cvref_t<R>>
                {
                    static_cast<Self&&>(snd),
                    static_cast<R&&>(recv)
                };
            }
        };

        template <typename... Ts>
        template <typename R>
        struct just_snd_<Ts...>::ops_<R>::type
        {
            just_snd<Ts...> snd;
            R recv;

            friend void tag_invoke(start_t, type& self) noexcept
            {
                std::apply([&]<typename... Us>(Us&&... args)
                {
                    exec::set_value(std::move(self.recv), static_cast<Us&&>(args)...);
                }, std::move(self.snd.values));
            }
        };

        // exec::just_error
        template <typename E>
        struct just_err_snd_
        {
            struct type;

            template <typename R>
            struct ops_
            {
                struct type;
            };

            template <typename R>
            using ops = typename ops_<R>::type;
        };

        template <typename E>
        using just_err_snd = typename just_err_snd_<E>::type;

        template <typename E>
        struct just_err_snd_<E>::type :
            completion_signatures<set_error_t(E)>
        {
            E error;

            template <forwarding<type> Self, receiver<E> R>
                requires std::constructible_from<E, copy_cvref_t<Self, E>>
            friend auto tag_invoke(connect_t, Self&& snd, R&& recv)
            {
                return ops<std::remove_cvref_t<R>>
                {
                    static_cast<Self&&>(snd),
                    static_cast<R&&>(recv)
                };
            }
        };

        template <typename E>
        template <typename R>
        struct just_err_snd_<E>::ops_<R>::type
        {
            just_err_snd<E> snd;
            R recv;

            friend void tag_invoke(start_t, type& self) noexcept
            {
                exec::set_error(std::move(self.recv), static_cast<E&&>(self.snd.error));
            }
        };

        // exec::just_done
        struct just_stopped_snd : completion_signatures<set_stopped_t()>
        {
            template <typename R>
            struct ops_
            {
                struct type;
            };

            template <forwarding<just_stopped_snd> Self, receiver R>
            friend auto tag_invoke(connect_t, Self&&, R&& recv)
            {
                return ops_<std::remove_cvref_t<R>>{ static_cast<R&&>(recv) };
            }
        };

        template <typename R>
        struct just_stopped_snd::ops_<R>::type
        {
            R recv;

            friend void tag_invoke(start_t, type& self) noexcept
            {
                exec::set_stopped(std::move(self.recv));
            }
        };
    }

    inline struct just_t
    {
        // @formatter:off
        template <movable_value... Ts>
        constexpr sender auto operator()(Ts&&... values) const
            noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
        {
            return detail::just_snd<std::decay_t<Ts>...>{
                {},
                { static_cast<Ts&&>(values)... }
            };
        }
        // @formatter:on
    } constexpr just{};

    inline struct just_error_t
    {
        // @formatter:off
        template <movable_value E>
        constexpr sender auto operator()(E&& error) const
            noexcept(std::is_nothrow_move_constructible_v<E>)
        {
            return detail::just_err_snd<std::decay_t<E>>{ {}, static_cast<E&&>(error) };
        }
        // @formatter:on
    } constexpr just_error{};

    inline struct just_stopped_t
    {
        constexpr sender auto operator()() const noexcept { return detail::just_stopped_snd{}; }
    } constexpr just_stopped{};
}
