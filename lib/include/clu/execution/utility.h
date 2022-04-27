#pragma once

#include <array>

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

    // Re-implement some of the algebraic types so that they support immovable types properly
    // Most of the functionalities are just missing for convenience's sake
    namespace detail
    {
        template <typename T>
        class ops_optional
        {
        public:
            ops_optional() noexcept: dummy_{} {}
            ops_optional(const ops_optional&) = delete;
            ~ops_optional() noexcept { reset(); }

            // clang-format off
            template <typename F>
                requires std::same_as<T, call_result_t<F>>
            explicit ops_optional(F&& func):
                engaged_(true), value_(static_cast<F&&>(func)()) {}
            // clang-format on

            void reset() noexcept
            {
                if (engaged_)
                {
                    engaged_ = false;
                    value_.~T();
                }
            }

            template <typename F>
                requires(std::same_as<T, call_result_t<F>>)
            T& emplace_with(F&& func)
            {
                engaged_ = true;
                T* ptr = new (std::addressof(value_)) T(static_cast<F&&>(func)());
                return *ptr;
            }

            explicit operator bool() const noexcept { return engaged_; }
            T& operator*() noexcept { return value_; }
            const T& operator*() const noexcept { return value_; }

        private:
            bool engaged_ = false;
            union
            {
                monostate dummy_;
                T value_;
            };
        };

        namespace ops_tpl
        {
            template <std::size_t I, typename T>
            struct leaf
            {
                CLU_NO_UNIQUE_ADDRESS T value;
            };

            template <typename IndType>
            using leaf_of = leaf<IndType::index, typename IndType::type>;

            template <typename... Ts>
            class type : leaf_of<Ts>...
            {
            public:
                type(const type&) = delete;
                type(type&&) = delete;
                ~type() noexcept = default;

                // clang-format off
                template <typename... Fn>
                explicit type(Fn&&... func):
                    leaf_of<Ts>{static_cast<Fn&&>(func)()}... {}
                // clang-format on

            private:
                template <forwarding<type> Self, typename Fn>
                friend decltype(auto) apply(Fn&& func, Self&& self)
                {
                    return static_cast<Fn&&>(func)( //
                        static_cast<copy_cvref_t<Self&&, leaf_of<Ts>>>(self).value...);
                }
            };

            template <typename... Ts, std::size_t... Is>
            type<meta::indexed_type<Is, Ts>...> get_type(std::index_sequence<Is...>);
        } // namespace ops_tpl

        template <typename... Ts>
        using ops_tuple = decltype(ops_tpl::get_type<Ts...>(std::index_sequence_for<Ts...>{}));

        template <typename... Ts>
        class ops_variant // NOLINT(cppcoreguidelines-pro-type-member-init)
        {
        public:
            ops_variant() requires(sizeof...(Ts) > 0) &&
                (std::default_initializable<meta::nth_type_q<0>::fn<Ts...>>) = default;

            template <std::size_t I, typename... Us>
            explicit ops_variant(std::in_place_index_t<I>, Us&&... args): index_(I)
            {
                using type = typename meta::nth_type_q<I>::template fn<Ts...>;
                new (data_) type(static_cast<Us&&>(args)...);
            }

            ~ops_variant() noexcept { destruct(); }

            std::size_t index() const noexcept { return index_; }

            template <std::size_t I, typename... Us>
            auto& emplace(Us&&... args)
            {
                destruct();
                index_ = sizeof...(Ts);
                using type = typename meta::nth_type_q<I>::template fn<Ts...>;
                type* ptr = new (data_) type(static_cast<Us&&>(args)...);
                index_ = I;
                return *ptr;
            }

            template <typename T, typename... Us>
            auto& emplace(Us&&... args)
            {
                destruct();
                index_ = sizeof...(Ts);
                T* ptr = new (data_) T(static_cast<Us&&>(args)...);
                index_ = meta::find_q<T>::template fn<Ts...>::value;
                return *ptr;
            }

        private:
            // ReSharper disable once CppUninitializedNonStaticDataMember
            std::size_t index_ = 0;
            alignas(Ts...) std::byte data_[std::max({sizeof(Ts)...})];

            template <std::size_t I, forwarding<ops_variant<Ts...>> Self>
            friend auto&& get(Self&& self)
            {
                if (self.index_ != I)
                    throw std::bad_variant_access();
                using type = typename meta::nth_type_q<I>::template fn<Ts...>;
                return get_unchecked<type>(static_cast<Self&&>(self));
            }

            template <typename T, forwarding<ops_variant<Ts...>> Self>
            friend auto&& get_unchecked(Self&& self)
            {
                using copy_cv = conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const T, T>;
                return static_cast<copy_cvref_t<Self, T>&&>(*reinterpret_cast<copy_cv*>(self.data_));
            }

            template <typename Fn, forwarding<ops_variant<Ts...>> Self>
            friend decltype(auto) visit(Fn&& visitor, Self&& self)
            {
                if (self.index_ == sizeof...(Ts))
                    throw std::bad_variant_access();
                static constexpr std::array vtbl{visit_as<Ts, Fn, Self>...};
                return vtbl[self.index_](static_cast<Fn&&>(visitor), static_cast<Self&&>(self));
            }

            template <typename T, typename Fn, forwarding<ops_variant<Ts...>> Self>
            static decltype(auto) visit_as(Fn&& visitor, Self&& self)
            {
                return static_cast<Fn&&>(visitor)(get_unchecked<T>(static_cast<Self&&>(self)));
            }

            void destruct() noexcept
            {
                if (index_ == sizeof...(Ts))
                    return;
                using fptr = void (*)(ops_variant&) noexcept;
                static constexpr std::array<fptr, sizeof...(Ts)> dtors{destruct_as<Ts>...};
                dtors[index_](*this);
            }

            template <typename T>
            static void destruct_as(ops_variant& self) noexcept
            {
                T& value = *reinterpret_cast<T*>(self.data_);
                value.~T();
            }
        };
    } // namespace detail
} // namespace clu::exec
