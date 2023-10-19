#pragma once

#include <array>

#include "execution_traits.h"

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
            concept has_set_value =
                requires(R&& recv, Args&&... args) { static_cast<R&&>(recv).set_value(static_cast<Args&&>(args)...); };

            template <typename R, typename E>
            concept has_set_error =
                requires(R&& recv, E&& err) { static_cast<R&&>(recv).set_error(static_cast<E&&>(err)); };

            template <typename R>
            concept has_set_stopped = requires(R&& recv) { static_cast<R&&>(recv).set_stopped(); };

            template <typename R>
            concept has_get_env = requires(const R& recv) { recv.get_env(); };

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
                using is_receiver = void;
                using set_value = void;
                using set_error = void;
                using set_stopped = void;
                using get_env = void;

                type()
                    requires(std::same_as<Base, no_base>) || std::is_default_constructible_v<Base>
                = default;

                // clang-format off
                template <typename B> requires
                    (!std::same_as<Base, no_base>) &&
                    std::constructible_from<Base, B>
                explicit type(B&& base) noexcept(std::is_nothrow_constructible_v<Base, B>):
                    base_(static_cast<B&&>(base)) {}

                Base& base() & noexcept requires has_base { return base_; }
                const Base& base() const& noexcept requires has_base { return base_; }
                Base&& base() && noexcept requires has_base { return std::move(base_); }
                const Base&& base() const&& noexcept requires has_base { return std::move(base_); }
                // clang-format on

            private:
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
                constexpr friend decltype(auto) tag_invoke(get_env_t, const D& self)
                {
                    if constexpr (has_get_env<Derived>)
                        return self.get_env();
                    else
                        return clu::adapt_env(clu::get_env(get_base(self))); // Only forward forwarding_query-s
                }
            };

            template <class_type Derived, typename Base = no_base>
            using receiver_adaptor = typename receiver_adaptor_<Derived, Base>::type;
        } // namespace recv_adpt
    } // namespace detail

    using detail::recv_adpt::receiver_adaptor;

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
            if constexpr (requires(P2& other) { other.unhandled_stopped(); })
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

    namespace detail
    {
        // Re-implement some of the algebraic types so that they support immovable types properly
        // Most of the functionalities are just missing for convenience's sake

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
            // clang-format off
            template <std::size_t I, typename T> struct leaf { T value; };
            template <typename... Ts> struct tuple_ { struct type; };
            template <typename... Ts> using tuple = typename tuple_<Ts...>::type;
            template <typename... Ts, std::size_t... Is> tuple<leaf<Is, Ts>...> type_impl(std::index_sequence<Is...>);
            // clang-format on

            template <typename... Ts>
            struct tuple_<Ts...>::type : Ts...
            {
                template <forwarding<type> Self, typename Fn>
                constexpr friend decltype(auto) apply(Fn&& func, Self&& self)
                    CLU_SINGLE_RETURN(static_cast<Fn&&>(func)( //
                        static_cast<copy_cvref_t<Self&&, Ts>>(self).value...));
            };
        } // namespace ops_tpl

        template <typename... Ts>
        using ops_tuple = decltype(ops_tpl::type_impl<Ts...>(std::index_sequence_for<Ts...>{}));

        template <typename... Fs>
        constexpr auto make_ops_tuple_from(Fs&&... fns)
            CLU_SINGLE_RETURN(ops_tuple<call_result_t<Fs>...>{{static_cast<Fs&&>(fns)()}...});

        template <typename... Ts>
        class ops_variant // NOLINT(cppcoreguidelines-pro-type-member-init)
        {
        public:
            ops_variant()
                requires(sizeof...(Ts) > 0) && (std::default_initializable<meta::nth_type_q<0>::fn<Ts...>>)
            = default;

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
            alignas(Ts...) std::byte data_[(std::max)({sizeof(Ts)...})];

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

        // Algorithm fundamentals

        template <typename... Sigs>
        using filtered_sigs =
            meta::unpack_invoke<meta::remove_q<void>::fn<Sigs...>, meta::quote<completion_signatures>>;

        template <typename Sigs, typename New>
        using add_sig = meta::unpack_invoke<meta::push_back_unique_l<Sigs, New>, meta::quote<filtered_sigs>>;

        template <typename Sigs, bool Nothrow = false>
        using maybe_add_throw = conditional_t<Nothrow, Sigs, add_sig<Sigs, set_error_t(std::exception_ptr)>>;

        template <typename... Ts>
        using nullable_variant = meta::unpack_invoke<meta::unique<std::monostate, Ts...>, meta::quote<std::variant>>;

        template <typename... Ts>
        using nullable_ops_variant = meta::unpack_invoke<meta::unique<std::monostate, Ts...>, meta::quote<ops_variant>>;

        template <typename Cpo, typename SetCpo, typename S, typename... Args>
        concept customized_sender_algorithm = //
            tag_invocable<Cpo, completion_scheduler_of_t<SetCpo, S>, S, Args...> || //
            tag_invocable<Cpo, S, Args...>;

        template <typename Cpo, typename SetCpo, typename S, typename... Args>
        auto invoke_customized_sender_algorithm(S&& snd, Args&&... args)
        {
            if constexpr (requires { requires tag_invocable<Cpo, completion_scheduler_of_t<SetCpo, S>, S, Args...>; })
            {
                using schd_t = completion_scheduler_of_t<SetCpo, S>;
                static_assert(sender<tag_invoke_result_t<Cpo, schd_t, S, Args...>>,
                    "customizations for sender algorithms should return senders");
                return tag_invoke(Cpo{}, exec::get_completion_scheduler<SetCpo>(snd), static_cast<S&&>(snd),
                    static_cast<Args&&>(args)...);
            }
            else // if constexpr (tag_invocable<Cpo, S, Args...>)
            {
                static_assert(sender<tag_invoke_result_t<Cpo, S, Args...>>,
                    "customizations for sender algorithms should return senders");
                return tag_invoke(Cpo{}, static_cast<S&&>(snd), static_cast<Args&&>(args)...);
            }
        }

        // Environments with stop tokens that are actually stoppable
        template <typename Env>
        concept stoppable_env = (!unstoppable_token<stop_token_of_t<Env>>);

        namespace schd_env
        {
            template <typename Schd>
            class env_t_
            {
            public:
                template <forwarding<Schd> S = Schd>
                explicit env_t_(S&& schd) noexcept(std::is_nothrow_constructible_v<Schd, S>):
                    schd_(static_cast<S&&>(schd))
                {
                }

                Schd tag_invoke(get_completion_scheduler_t<set_value_t>) const noexcept { return schd_; }
                Schd tag_invoke(get_completion_scheduler_t<set_stopped_t>) const noexcept { return schd_; }

            private:
                Schd schd_;
            };
        } // namespace schd_env

        template <typename Schd>
        using comp_schd_env = schd_env::env_t_<std::remove_cvref_t<Schd>>;
    } // namespace detail
} // namespace clu::exec
