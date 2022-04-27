#pragma once

#include <optional>

#include "execution_traits.h"
#include "utility.h"
#include "run_loop.h"
#include "../piper.h"
#include "../copy_elider.h"

namespace clu::exec
{
    namespace detail
    {
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

        // clang-format off
        template <typename Cpo, typename SetCpo, typename S, typename... Args>
        concept customized_sender_algorithm =
            tag_invocable<Cpo, completion_scheduler_of_t<SetCpo, S>, S, Args...> ||
            tag_invocable<Cpo, S, Args...>;
        // clang-format on

        template <typename Cpo, typename SetCpo, typename S, typename... Args>
        auto invoke_customized_sender_algorithm(Cpo, S&& snd, Args&&... args)
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

        namespace just
        {
            template <typename R, typename Cpo, typename... Ts>
            struct ops_t_
            {
                class type;
            };

            template <typename R, typename Cpo, typename... Ts>
            class ops_t_<R, Cpo, Ts...>::type
            {
            public:
                // clang-format off
                template <typename Tup>
                type(R&& recv, Tup&& values) noexcept(
                    std::is_nothrow_move_constructible_v<R> &&
                    std::is_nothrow_constructible_v<std::tuple<Ts...>, Tup>):
                    recv_(static_cast<R&&>(recv)),
                    values_(static_cast<Tup&&>(values)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS R recv_;
                CLU_NO_UNIQUE_ADDRESS std::tuple<Ts...> values_;

                friend void tag_invoke(start_t, type& ops) noexcept
                {
                    std::apply([&](Ts&&... values)
                        { Cpo{}(static_cast<type&&>(ops).recv_, static_cast<Ts&&>(values)...); },
                        static_cast<type&&>(ops).values_);
                }
            };

            template <typename R, typename Cpo, typename... Ts>
            using ops_t = typename ops_t_<R, Cpo, Ts...>::type;

            template <typename Cpo, typename... Ts>
            struct snd_t_
            {
                class type;
            };

            template <typename Cpo, typename... Ts>
            class snd_t_<Cpo, Ts...>::type
            {
            public:
                // clang-format off
                template <typename... Us>
                constexpr explicit type(Us&&... args) noexcept(
                    std::is_nothrow_constructible_v<std::tuple<Ts...>, Us...>):
                    values_(static_cast<Us&&>(args)...) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS std::tuple<Ts...> values_;

                template <typename R, forwarding<type> Self>
                friend auto tag_invoke(connect_t, Self&& snd, R&& recv)
                    CLU_SINGLE_RETURN(ops_t<R, Cpo, Ts...>{static_cast<R&&>(recv), static_cast<Self&&>(snd).values_});

                friend completion_signatures<Cpo(Ts...)> tag_invoke(
                    get_completion_signatures_t, const type&, auto) noexcept
                {
                    return {};
                }
            };

            template <typename Cpo, typename... Ts>
            using snd_t = typename snd_t_<Cpo, Ts...>::type;

            struct just_t
            {
                template <typename... Ts>
                auto operator()(Ts&&... values) const
                {
                    return snd_t<set_value_t, std::decay_t<Ts>...>(static_cast<Ts&&>(values)...);
                }
            };

            struct just_error_t
            {
                template <typename E>
                auto operator()(E&& error) const
                {
                    return snd_t<set_error_t, std::decay_t<E>>(static_cast<E&&>(error));
                }
            };

            struct just_stopped_t
            {
                constexpr auto operator()() const noexcept { return snd_t<set_stopped_t>(); }
            };
        } // namespace just

        namespace stop_req
        {
            template <typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename R>
            using ops_t = typename ops_t_<std::remove_cvref_t<R>>::type;

            template <typename R>
            class ops_t_<R>::type
            {
            public:
                template <typename R2>
                explicit type(R2&& recv): recv_(static_cast<R2&&>(recv))
                {
                }

            private:
                R recv_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    if (get_stop_token(get_env(self.recv_)).stop_requested())
                        exec::set_stopped(static_cast<R&&>(self.recv_));
                    else
                        exec::set_value(static_cast<R&&>(self.recv_));
                }
            };

            struct snd_t
            {
                template <typename R>
                friend auto tag_invoke(connect_t, snd_t, R&& recv) noexcept(std::is_nothrow_move_constructible_v<R>)
                {
                    if constexpr (unstoppable_token<stop_token_of_t<env_of_t<R>>>)
                        return exec::connect(just::just_t{}, static_cast<R&&>(recv));
                    else
                        return ops_t<R>(static_cast<R&&>(recv));
                }

                template <typename Env>
                friend auto tag_invoke(get_completion_signatures_t, snd_t, Env) noexcept
                {
                    if constexpr (unstoppable_token<stop_token_of_t<Env>>)
                        return completion_signatures<set_value_t()>{};
                    else
                        return completion_signatures<set_value_t(), set_stopped_t()>{};
                }
            };

            struct stop_if_requested_t
            {
                constexpr auto operator()() const noexcept { return snd_t{}; }
            };
        }

        namespace upon
        {
            template <typename T, bool NoThrow = false>
            using sigs_of_single = completion_signatures<
                conditional_t<std::is_void_v<T>, set_value_t(), set_value_t(with_regular_void_t<T>)>>;

            template <typename F, typename... Args>
            using sigs_of_inv =
                maybe_add_throw<sigs_of_single<std::invoke_result_t<F, Args...>>, nothrow_invocable<F, Args...>>;

            template <typename R, typename Cpo, typename F>
            struct recv_t_
            {
                class type;
            };

            template <typename R, typename Cpo, typename F>
            using recv_t = typename recv_t_<std::remove_cvref_t<R>, Cpo, F>::type;

            template <typename R, typename Cpo, typename F>
            class recv_t_<R, Cpo, F>::type
            {
            public:
                // clang-format off
                template <typename R2, typename F2>
                type(R2&& recv, F2&& func) noexcept(
                    std::is_nothrow_constructible_v<R, R2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    recv_(static_cast<R2&&>(recv)),
                    func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS R recv_;
                CLU_NO_UNIQUE_ADDRESS F func_;

                // Transformed
                template <typename... Args>
                    requires std::invocable<F, Args...> && receiver_of<R, sigs_of_inv<F, Args...>>
                friend void tag_invoke(Cpo, type&& self, Args&&... args) noexcept
                {
                    // clang-format off
                    if constexpr (nothrow_invocable<F, Args...>)
                        self.set_impl(static_cast<Args&&>(args)...);
                    else
                        try { self.set_impl(static_cast<Args&&>(args)...); }
                        catch (...) { exec::set_error(static_cast<R&&>(self.recv_), std::current_exception()); }
                    // clang-format on
                }

                // Pass through
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return exec::get_env(self.recv_); }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.recv_, static_cast<Args&&>(args)...);
                }

                // clang-format off
                template <recvs::completion_cpo Tag, typename... Args> requires
                    (!std::same_as<Tag, Cpo>) &&
                    receiver_of<R, completion_signatures<Tag(Args...)>>
                constexpr friend void tag_invoke(Tag cpo, type&& self, Args&&... args) noexcept
                {
                    cpo(static_cast<R&&>(self.recv_), static_cast<Args&&>(args)...);
                }
                // clang-format on

                template <typename... Args>
                constexpr void set_impl(Args&&... args) noexcept(nothrow_invocable<F, Args...>)
                {
                    if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>)
                    {
                        std::invoke(std::move(func_), static_cast<Args&&>(args)...);
                        exec::set_value(static_cast<R&&>(recv_));
                    }
                    else
                    {
                        exec::set_value(
                            static_cast<R&&>(recv_), std::invoke(std::move(func_), static_cast<Args&&>(args)...));
                    }
                }
            };

            template <typename S, typename Cpo, typename F>
            struct snd_t_
            {
                class type;
            };

            struct make_sig_impl_base
            {
                template <typename... Ts>
                using value_sig = completion_signatures<set_value_t(Ts...)>;
                template <typename E>
                using error_sig = completion_signatures<set_error_t(E)>;
                using stopped_sig = completion_signatures<set_stopped_t()>;
            };
            template <typename Cpo, typename F>
            struct make_sig_impl
            {
            };
            template <typename F>
            struct make_sig_impl<set_value_t, F> : make_sig_impl_base
            {
                template <typename... Ts>
                using value_sig = sigs_of_inv<F, Ts...>;
            };
            template <typename F>
            struct make_sig_impl<set_error_t, F> : make_sig_impl_base
            {
                template <typename E>
                using error_sig = sigs_of_inv<F, E>;
            };
            template <typename F>
            struct make_sig_impl<set_stopped_t, F> : make_sig_impl_base
            {
                using stopped_sig = sigs_of_inv<F>;
            };

            template <typename S, typename Cpo, typename F>
            class snd_t_<S, Cpo, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename F2>
                constexpr type(S2&& snd, F2&& func) noexcept(
                    std::is_nothrow_constructible_v<S, S2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    snd_(static_cast<S2&&>(snd)),
                    func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
                CLU_NO_UNIQUE_ADDRESS F func_;

                template <forwarding<type> Self, typename R>
                    requires sender_to<copy_cvref_t<Self, S>, recv_t<R, Cpo, F>>
                friend auto tag_invoke(connect_t, Self&& snd, R&& recv) noexcept(
                    conn::nothrow_connectable<S, recv_t<R, Cpo, F>>)
                {
                    return exec::connect(static_cast<Self&&>(snd).snd_,
                        recv_t<R, Cpo, F>(static_cast<R&&>(recv), static_cast<Self&&>(snd).func_));
                }

                template <snd_qry::fwd_snd_query Q, typename... Args>
                    requires callable<Q, const S&, Args...>
                constexpr friend auto tag_invoke(Q, const type& snd, Args&&... args) noexcept(
                    nothrow_callable<Q, const S&, Args...>)
                {
                    return Q{}(snd.snd_, static_cast<Args&&>(args)...);
                }

                using make_sig = make_sig_impl<Cpo, F>;

                template <typename Env>
                constexpr friend make_completion_signatures<S, Env, completion_signatures<>,
                    make_sig::template value_sig, make_sig::template error_sig, typename make_sig::stopped_sig>
                tag_invoke(get_completion_signatures_t, type&&, Env) noexcept
                {
                    return {};
                }
            };

            template <typename S, typename Cpo, typename F>
            using snd_t = typename snd_t_<std::remove_cvref_t<S>, Cpo, std::decay_t<F>>::type;

            template <typename UponCpo, typename SetCpo>
            struct upon_t
            {
                template <sender S, typename F>
                constexpr auto operator()(S&& snd, F&& func) const
                {
                    if constexpr (customized_sender_algorithm<UponCpo, SetCpo, S, F>)
                        return detail::invoke_customized_sender_algorithm<UponCpo, SetCpo>(
                            static_cast<S&&>(snd), static_cast<F&&>(func));
                    else
                        return snd_t<S, SetCpo, F>(static_cast<S&&>(snd), static_cast<F&&>(func));
                }

                template <typename F>
                constexpr auto operator()(F&& func) const
                {
                    return clu::make_piper(clu::bind_back(UponCpo{}, static_cast<F&&>(func)));
                }
            };

            // clang-format off
            struct then_t : upon_t<then_t, set_value_t> {};
            struct upon_error_t : upon_t<upon_error_t, set_error_t> {};
            struct upon_stopped_t : upon_t<upon_stopped_t, set_stopped_t> {};
            // clang-format on
        } // namespace upon

        namespace let
        {
            template <typename T>
            using decay_lref = std::decay_t<T>&;

            template <typename S, typename R, typename Cpo, typename F>
            struct storage;

            template <typename S, typename R, typename F>
            struct storage<S, R, set_value_t, F>
            {
                template <typename... Args>
                using ops_for = connect_result_t<std::invoke_result_t<F, decay_lref<Args>...>, R>;

                value_types_of_t<S, env_of_t<R>, decayed_tuple, nullable_variant> args;
                value_types_of_t<S, env_of_t<R>, ops_for, nullable_ops_variant> ops;
            };

            template <typename S, typename R, typename F>
            struct storage<S, R, set_error_t, F>
            {
                template <typename... Es>
                using args_variant_for = nullable_variant<decayed_tuple<Es>...>;
                template <typename... Es>
                using ops_variant_for =
                    nullable_ops_variant<connect_result_t<std::invoke_result_t<F, decay_lref<Es>>, R>...>;

                error_types_of_t<S, env_of_t<R>, args_variant_for> args;
                error_types_of_t<S, env_of_t<R>, ops_variant_for> ops;
            };

            template <typename S, typename R, typename F>
            struct storage<S, R, set_stopped_t, F>
            {
                std::variant<std::tuple<>> args;
                ops_variant<std::monostate, connect_result_t<std::invoke_result_t<F>, R>> ops;
            };

            template <typename S, typename R, typename Cpo, typename F>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R, typename Cpo, typename F>
            using ops_t = typename ops_t_<S, std::remove_cvref_t<R>, Cpo, F>::type;

            template <typename F, typename Env, typename... Args>
            using recv_sigs = completion_signatures_of_t<std::invoke_result_t<F, decay_lref<Args>...>, Env>;

            template <typename S, typename R, typename Cpo, typename F>
            struct recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename Cpo, typename F>
            using recv_t = typename recv_t_<S, std::remove_cvref_t<R>, Cpo, F>::type;

            template <typename S, typename R, typename Cpo, typename F>
            class recv_t_<S, R, Cpo, F>::type
            {
            public:
                explicit type(ops_t<S, R, Cpo, F>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, R, Cpo, F>* ops_;

                R&& recv() const noexcept { return static_cast<R&&>(ops_->recv_); }
                F&& func() const noexcept { return static_cast<F&&>(ops_->func_); }

                template <typename... Args>
                void set_impl(Args&&... args)
                {
                    // Store the args
                    auto& tuple =
                        ops_->storage_.args.template emplace<decayed_tuple<Args...>>(static_cast<Args&&>(args)...);
                    // Produce the sender and connect
                    const auto ce = [&] { return exec::connect(std::apply(func(), tuple), recv()); };
                    exec::start(ops_->storage_.ops.template emplace<call_result_t<decltype(ce)>>(copy_elider{ce}));
                }

                // Transformed
                template <typename... Args>
                    requires std::invocable<F, Args...> && receiver_of<R, recv_sigs<F, env_of_t<R>, Args...>>
                friend void tag_invoke(Cpo, type&& self, Args&&... args) noexcept
                {
                    // clang-format off
                    if constexpr (receiver_of<R, completion_signatures<set_error_t(std::exception_ptr)>>)
                        try { self.set_impl(static_cast<Args&&>(args)...); }
                        catch (...) { exec::set_error(self.recv(), std::current_exception()); }
                    // clang-format on
                    else
                        self.set_impl(static_cast<Args&&>(args)...); // Shouldn't throw theoretically
                }

                // Pass through
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return exec::get_env(self.recv()); }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.recv(), static_cast<Args&&>(args)...);
                }

                // clang-format off
                template <recvs::completion_cpo Tag, typename... Args> requires
                    (!std::same_as<Tag, Cpo>) &&
                    receiver_of<R, completion_signatures<Tag(Args...)>>
                constexpr friend void tag_invoke(Tag cpo, type&& self, Args&&... args) noexcept
                {
                    cpo(self.recv(), static_cast<Args&&>(args)...);
                }
                // clang-format on
            };

            template <typename S, typename R, typename Cpo, typename F>
            class ops_t_<S, R, Cpo, F>::type
            {
            public:
                // clang-format off
                template <typename R2, typename F2>
                type(S&& snd, R2&& recv, F2&& func) noexcept(
                    conn::nothrow_connectable<S, recv_t<S, R, Cpo, F>> &&
                    std::is_nothrow_constructible_v<R, R2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    initial_ops_(exec::connect(static_cast<S&&>(snd), recv_t<S, R, Cpo, F>(this))),
                    recv_(static_cast<R2&&>(recv)), func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                friend recv_t<S, R, Cpo, F>;

                connect_result_t<S, recv_t<S, R, Cpo, F>> initial_ops_;
                R recv_;
                CLU_NO_UNIQUE_ADDRESS F func_;
                storage<S, R, Cpo, F> storage_;

                friend void tag_invoke(start_t, type& self) noexcept { exec::start(self.initial_ops_); }
            };

            struct make_sig_impl_base
            {
                template <typename... Ts>
                using value_sig = completion_signatures<set_value_t(Ts...)>;
                template <typename E>
                using error_sig = completion_signatures<set_error_t(E)>;
                using stopped_sig = completion_signatures<set_stopped_t()>;
            };
            template <typename Cpo, typename Env, typename F>
            struct make_sig_impl
            {
            };

            template <typename F, typename Env>
            struct make_sig_impl<set_value_t, Env, F> : make_sig_impl_base
            {
                template <typename... Ts>
                using value_sig = recv_sigs<F, Env, Ts...>;
            };
            template <typename F, typename Env>
            struct make_sig_impl<set_error_t, Env, F> : make_sig_impl_base
            {
                template <typename E>
                using error_sig = recv_sigs<F, Env, E>;
            };
            template <typename F, typename Env>
            struct make_sig_impl<set_stopped_t, Env, F> : make_sig_impl_base
            {
                using stopped_sig = recv_sigs<F, Env>;
            };

            template <typename S, typename Cpo, typename F>
            struct snd_t_
            {
                class type;
            };

            template <typename S, typename Cpo, typename F>
            using snd_t = typename snd_t_<std::remove_cvref_t<S>, Cpo, std::decay_t<F>>::type;

            template <typename S, typename Cpo, typename F>
            class snd_t_<S, Cpo, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename F2>
                type(S2&& snd, F2&& func) noexcept(
                    std::is_nothrow_constructible_v<S, S2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    snd_(static_cast<S2&&>(snd)), func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
                CLU_NO_UNIQUE_ADDRESS F func_;

                template <forwarding<type> Self, typename R>
                    requires sender_to<copy_cvref_t<Self, S>, recv_t<S, R, Cpo, F>>
                friend auto tag_invoke(connect_t, Self&& self, R&& recv) CLU_SINGLE_RETURN(ops_t<S, R, Cpo, F>(
                    static_cast<Self&&>(self).snd_, static_cast<R&&>(recv), static_cast<Self&&>(self).func_));

                // clang-format off
                template <typename Env>
                constexpr friend make_completion_signatures<S, Env,
                    completion_signatures<set_error_t(std::exception_ptr)>,
                    make_sig_impl<Cpo, Env, F>::template value_sig,
                    make_sig_impl<Cpo, Env, F>::template error_sig,
                    typename make_sig_impl<Cpo, Env, F>::stopped_sig>
                tag_invoke(get_completion_signatures_t, const type&, Env) noexcept { return {}; }
                // clang-format on
            };

            template <typename LetCpo, typename SetCpo>
            struct let_t
            {
                // clang-format off
                template <sender S, typename F> requires
                    (!std::same_as<SetCpo, set_stopped_t>) ||
                    std::invocable<F>
                auto operator()(S&& snd, F&& func) const
                // clang-format on
                {
                    if constexpr (customized_sender_algorithm<LetCpo, SetCpo, S, F>)
                        return detail::invoke_customized_sender_algorithm<LetCpo, SetCpo>(
                            static_cast<S&&>(snd), static_cast<F&&>(func));
                    else
                        return snd_t<S, SetCpo, F>(static_cast<S&&>(snd), static_cast<F&&>(func));
                }

                template <typename F>
                constexpr auto operator()(F&& func) const
                {
                    return clu::make_piper(clu::bind_back(LetCpo{}, static_cast<F&&>(func)));
                }
            };

            // clang-format off
            struct let_value_t : let_t<let_value_t, set_value_t> {};
            struct let_error_t : let_t<let_error_t, set_error_t> {};
            struct let_stopped_t : let_t<let_stopped_t, set_stopped_t> {};
            // clang-format on
        } // namespace let

        namespace on
        {
            template <typename Env, typename Schd>
            using env_t = adapted_env_t<Env, get_scheduler_t, Schd>;

            template <typename Env, typename Schd>
            auto replace_schd(Env&& env, Schd&& schd) noexcept
            {
                return exec::make_env(static_cast<Env&&>(env), //
                    get_scheduler, static_cast<Schd&&>(schd));
            }

            template <typename S, typename R, typename Schd>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R, typename Schd>
            using ops_t = typename ops_t_<S, std::decay_t<R>, Schd>::type;

            template <typename S, typename R, typename Schd>
            struct recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename Schd>
            using recv_t = typename recv_t_<S, R, Schd>::type;

            template <typename S, typename R, typename Schd>
            class recv_t_<S, R, Schd>::type : public receiver_adaptor<type>
            {
            public:
                explicit type(ops_t<S, R, Schd>* ops) noexcept: ops_(ops) {}

                const R& base() const& noexcept;
                R&& base() && noexcept;
                void set_value() && noexcept;

            private:
                ops_t<S, R, Schd>* ops_;
            };

            template <typename S, typename R, typename Schd>
            struct recv2_t_
            {
                class type;
            };

            template <typename S, typename R, typename Schd>
            using recv2_t = typename recv2_t_<S, R, Schd>::type;

            template <typename S, typename R, typename Schd>
            class recv2_t_<S, R, Schd>::type : public receiver_adaptor<type>
            {
            public:
                explicit type(ops_t<S, R, Schd>* ops) noexcept: ops_(ops) {}

                const R& base() const& noexcept;
                R&& base() && noexcept;
                env_t<env_of_t<R>, Schd> get_env() const noexcept;

            private:
                ops_t<S, R, Schd>* ops_;
            };

            template <typename S, typename R, typename Schd>
            class ops_t_<S, R, Schd>::type
            {
            public:
                template <typename R2>
                type(S&& snd, R2&& recv, Schd&& schd):
                    snd_(static_cast<S&&>(snd)), recv_(static_cast<R2&&>(recv)), schd_(static_cast<Schd&&>(schd)),
                    inner_ops_(std::in_place_index<0>,
                        copy_elider{[&]
                            {
                                return exec::connect(exec::schedule(schd_), //
                                    recv_t<S, R, Schd>(this));
                            }})
                {
                }

            private:
                friend class recv_t_<S, R, Schd>::type;
                friend class recv2_t_<S, R, Schd>::type;
                using schd_ops_t = connect_result_t<schedule_result_t<Schd>, recv_t<S, R, Schd>>;

                S snd_;
                R recv_;
                Schd schd_;
                ops_variant< //
                    connect_result_t<schedule_result_t<Schd>, recv_t<S, R, Schd>>,
                    connect_result_t<S, recv2_t<S, R, Schd>>>
                    inner_ops_;

                friend void tag_invoke(start_t, type& self) noexcept { exec::start(get<0>(self.inner_ops_)); }
            };

            // clang-format off
            template <typename S, typename R, typename Schd>
            const R& recv_t_<S, R, Schd>::type::base() const & noexcept { return ops_->recv_; }
            template <typename S, typename R, typename Schd>
            R&& recv_t_<S, R, Schd>::type::base() && noexcept { return static_cast<R&&>(ops_->recv_); }
            template <typename S, typename R, typename Schd>
            const R& recv2_t_<S, R, Schd>::type::base() const & noexcept { return ops_->recv_; }
            template <typename S, typename R, typename Schd>
            R&& recv2_t_<S, R, Schd>::type::base() && noexcept { return static_cast<R&&>(ops_->recv_); }
            // clang-format on

            template <typename S, typename R, typename Schd>
            void recv_t_<S, R, Schd>::type::set_value() && noexcept
            {
                try
                {
                    auto* ops = ops_; // *this is going to be destroyed
                    auto& final_ops = ops->inner_ops_.template emplace<1>(copy_elider{[=]
                        {
                            return exec::connect(static_cast<S&&>(ops->snd_), //
                                recv2_t<S, R, Schd>(ops));
                        }}); // notice: *this is destroyed here
                    exec::start(final_ops);
                }
                catch (...)
                {
                    exec::set_error(std::move(*this).base(), std::current_exception());
                }
            }

            template <typename S, typename R, typename Schd>
            env_t<env_of_t<R>, Schd> recv2_t_<S, R, Schd>::type::get_env() const noexcept
            {
                return on::replace_schd(exec::get_env(base()), ops_->schd_);
            }

            template <typename Schd, typename Snd>
            struct snd_t_
            {
                class type;
            };

            template <typename Schd, typename Snd>
            using snd_t = typename snd_t_<std::decay_t<Schd>, std::decay_t<Snd>>::type;

            template <typename Schd, typename Snd>
            class snd_t_<Schd, Snd>::type
            {
            public:
                // clang-format off
                template <typename Schd2, typename Snd2>
                type(Schd2&& schd, Snd2&& snd):
                    schd_(static_cast<Schd2&&>(schd)), snd_(static_cast<Snd2&&>(snd)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS Schd schd_;
                Snd snd_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return ops_t<Snd, R, Schd>(
                        static_cast<Snd&&>(self.snd_), static_cast<R&&>(recv), static_cast<Schd&&>(self.schd_));
                }

                template <typename Env>
                constexpr friend make_completion_signatures< //
                    Snd, env_t<Env, Schd>, //
                    make_completion_signatures< //
                        schedule_result_t<Schd>, Env, completion_signatures<set_error_t(std::exception_ptr)>,
                        meta::constant_q<completion_signatures<>>::fn>>
                tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept
                {
                    return {};
                }
            };

            struct on_t
            {
                template <scheduler Schd, sender Snd>
                auto operator()(Schd&& schd, Snd&& snd) const
                {
                    if constexpr (tag_invocable<on_t, Schd, Snd>)
                    {
                        static_assert(
                            sender<tag_invoke_result_t<on_t, Schd, Snd>>, "customization of on should return a sender");
                        return clu::tag_invoke(*this, static_cast<Schd&&>(schd), static_cast<Snd&&>(snd));
                    }
                    else
                        return snd_t<Schd, Snd>(static_cast<Schd&&>(schd), static_cast<Snd&&>(snd));
                }
            };
        } // namespace on

        namespace schd_from
        {
            template <typename Cpo, typename... Ts>
            std::tuple<Cpo, std::decay_t<Ts>...> storage_tuple_impl(Cpo (*)(Ts...));
            template <typename Sig>
            using storage_tuple_t = decltype(schd_from::storage_tuple_impl(static_cast<Sig*>(nullptr)));
            template <typename S, typename R>
            using storage_variant_t = meta::unpack_invoke<
                meta::transform_l<completion_signatures_of_t<S, env_of_t<R>>, meta::quote1<storage_tuple_t>>,
                meta::quote<nullable_variant>>;

            template <typename S, typename R, typename Schd>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R, typename Schd>
            using ops_t = typename ops_t_<S, std::remove_cvref_t<R>, Schd>::type;

            template <typename S, typename R, typename Schd>
            struct recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename Schd>
            using recv_t = typename recv_t_<S, std::remove_cvref_t<R>, Schd>::type;

            template <typename S, typename R, typename Schd>
            struct recv2_t_
            {
                class type;
            };

            template <typename S, typename R, typename Schd>
            using recv2_t = typename recv2_t_<S, R, Schd>::type;

            template <typename S, typename R, typename Schd>
            class recv_t_<S, R, Schd>::type
            {
            public:
                explicit type(ops_t<S, R, Schd>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, R, Schd>* ops_;

                recv2_t<S, R, Schd>&& recv() const noexcept;

                template <typename Cpo, typename... Args>
                void set_impl(Cpo, Args&&... args)
                {
                    using tuple_t = storage_tuple_t<Cpo(Args...)>;
                    // Storage the args
                    ops_->args_.template emplace<tuple_t>(Cpo{}, static_cast<Args&&>(args)...);
                    // Schedule and connect, and then start the operation
                    exec::start(ops_->final_ops_.emplace_with(
                        [&]
                        {
                            auto schd_snd = exec::schedule(static_cast<Schd&&>(ops_->schd_));
                            return exec::connect(std::move(schd_snd), recv());
                        }));
                }

                // Transformed
                template <typename Cpo, typename... Args>
                    requires receiver_of<R, completion_signatures<Cpo(Args...)>>
                friend void tag_invoke(Cpo, type&& self, Args&&... args) noexcept
                {
                    // clang-format off
                    try { self.set_impl(Cpo{}, static_cast<Args&&>(args)...); }
                    catch (...) { exec::set_error(self.recv(), std::current_exception()); }
                    // clang-format on
                }

                // Pass through
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return exec::get_env(self.recv()); }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.recv(), static_cast<Args&&>(args)...);
                }
            };

            template <typename S, typename R, typename Schd>
            class recv2_t_<S, R, Schd>::type : public receiver_adaptor<type, R>
            {
            public:
                // clang-format off
                template <typename R2>
                type(R2&& recv, ops_t<S, R, Schd>* ops):
                    receiver_adaptor<type, R>(static_cast<R2&&>(recv)), ops_(ops) {}
                // clang-format on

                // Only translate set_value, other channels are passed-through
                void set_value() && noexcept
                {
                    // clang-format off
                    std::visit(
                        [&]<typename Tup>(Tup&& tuple) noexcept
                        {
                            if constexpr (std::is_same_v<Tup, std::monostate>)
                                unreachable();
                            else
                                std::apply(
                                    [&]<typename Cpo, typename... Args>(Cpo, Args&&... args) noexcept
                                    {
                                        Cpo{}(std::move(this->base()), static_cast<Args&&>(args)...);
                                    }, static_cast<Tup&&>(tuple));
                        }, std::move(ops_->args_));
                    // clang-format on
                }

            private:
                ops_t<S, R, Schd>* ops_;
            };

            template <typename S, typename R, typename Schd>
            class ops_t_<S, R, Schd>::type
            {
            public:
                // clang-format off
                template <typename R2>
                type(S&& snd, R2&& recv, Schd schd):
                    initial_ops_(exec::connect(static_cast<S&&>(snd), recv_t<S, R, Schd>(this))),
                    recv_(static_cast<R2&&>(recv), this),
                    schd_(static_cast<Schd&&>(schd)) {}
                // clang-format on

            private:
                friend recv_t<S, R, Schd>;
                friend recv2_t<S, R, Schd>;

                using final_ops_t = connect_result_t<schedule_result_t<Schd>, recv2_t<S, R, Schd>>;

                connect_result_t<S, recv_t<S, R, Schd>> initial_ops_;
                recv2_t<S, R, Schd> recv_;
                CLU_NO_UNIQUE_ADDRESS Schd schd_;
                storage_variant_t<S, R> args_;
                ops_optional<final_ops_t> final_ops_;

                friend void tag_invoke(start_t, type& self) noexcept { exec::start(self.initial_ops_); }
            };

            template <typename S, typename R, typename Schd>
            recv2_t<S, R, Schd>&& recv_t_<S, R, Schd>::type::recv() const noexcept
            {
                return static_cast<recv2_t<S, R, Schd>&&>(ops_->recv_);
            }

            template <typename Schd, typename Env>
            using additional_sigs = make_completion_signatures<schedule_result_t<Schd>, Env,
                completion_signatures<set_error_t(std::exception_ptr)>, meta::constant_q<completion_signatures<>>::fn>;

            template <typename S, typename Schd>
            struct snd_t_
            {
                class type;
            };

            template <typename S, typename Schd>
            using snd_t = typename snd_t_<std::remove_cvref_t<S>, std::remove_cvref_t<Schd>>::type;

            template <typename S, typename Schd>
            class snd_t_<S, Schd>::type
            {
            public:
                // clang-format off
                template <typename S2, typename Schd2>
                type(S2&& snd, Schd2&& schd):
                    snd_(static_cast<S2&&>(snd)), schd_(static_cast<Schd2&&>(schd)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
                CLU_NO_UNIQUE_ADDRESS Schd schd_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv) CLU_SINGLE_RETURN(
                    ops_t<S, R, Schd>(static_cast<S&&>(self.snd_), static_cast<R&&>(recv), self.schd_));

                // clang-format off
                template <typename Env>
                constexpr friend make_completion_signatures<S, Env, additional_sigs<Schd, Env>>
                tag_invoke(get_completion_signatures_t, const type&, Env) noexcept { return {}; }
                // clang-format on

                template <snd_qry::fwd_snd_query Q, typename... Args>
                    requires callable<Q, const S&, Args...>
                constexpr friend auto tag_invoke(Q, const type& snd, Args&&... args) noexcept(
                    nothrow_callable<Q, const S&, Args...>)
                {
                    return Q{}(snd.snd_, static_cast<Args&&>(args)...);
                }

                // Customize completion scheduler
                constexpr friend const Schd& tag_invoke(
                    get_completion_scheduler_t<set_value_t>, const type& snd) noexcept
                {
                    return snd.schd_;
                }

                constexpr friend const Schd& tag_invoke(
                    get_completion_scheduler_t<set_stopped_t>, const type& snd) noexcept
                {
                    return snd.schd_;
                }

                constexpr friend void tag_invoke(get_completion_scheduler_t<set_error_t>, const type& snd) = delete;
            };

            struct schedule_from_t
            {
                template <scheduler Schd, sender Snd>
                auto operator()(Schd&& schd, Snd&& snd) const
                {
                    if constexpr (tag_invocable<schedule_from_t, Schd, Snd>)
                    {
                        static_assert(sender<tag_invoke_result_t<schedule_from_t, Schd, Snd>>,
                            "customization of schedule_from should return a sender");
                        return clu::tag_invoke(*this, static_cast<Schd&&>(schd), static_cast<Snd&&>(snd));
                    }
                    else
                        return snd_t<Snd, Schd>(static_cast<Snd&&>(snd), static_cast<Schd&&>(schd));
                }
            };
        } // namespace schd_from

        namespace xfer
        {
            struct transfer_t
            {
                template <sender Snd, scheduler Schd>
                auto operator()(Snd&& snd, Schd&& schd) const
                {
                    if constexpr (customized_sender_algorithm<transfer_t, set_value_t, Snd, Schd>)
                        return detail::invoke_customized_sender_algorithm<transfer_t, set_value_t>(
                            static_cast<Snd&&>(snd), static_cast<Schd&&>(schd));
                    else
                        return schd_from::schedule_from_t{}(static_cast<Schd&&>(schd), static_cast<Snd&&>(snd));
                }

                template <scheduler Schd>
                constexpr auto operator()(Schd&& schd) const noexcept
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<Schd&&>(schd)));
                }
            };

            struct transfer_just_t
            {
                template <scheduler Schd, typename... Ts>
                auto operator()(Schd&& schd, Ts&&... args) const
                {
                    if constexpr (tag_invocable<transfer_just_t, Schd, Ts...>)
                    {
                        static_assert(sender<tag_invoke_result_t<transfer_just_t, Schd, Ts...>>,
                            "customization of transfer_just should return a sender");
                        return clu::tag_invoke(*this, static_cast<Schd&&>(schd), static_cast<Ts&&>(args)...);
                    }
                    else
                        return just::just_t{}(static_cast<Ts&&>(args)...) | transfer_t{}(static_cast<Schd&&>(schd));
                }
            };
        } // namespace xfer

        namespace bulk
        {
            // TODO: implement
        } // namespace bulk

        namespace split
        {
            // TODO: implement
        } // namespace split

        namespace when_all
        {
            template <typename Env>
            using env_t = adapted_env_t<Env, get_stop_token_t, in_place_stop_token>;

            template <typename... Ts>
            using one_or_zero_type = std::bool_constant<(sizeof...(Ts) < 2)>;
            template <typename... Ts>
            using sig_of_decayed_rref = set_value_t(std::decay_t<Ts>&&...);
            template <typename R, typename... Ts>
            inline constexpr bool can_send_value =
                meta::none_of_q<>::value<value_types_of_t<Ts, env_t<env_of_t<R>>, decayed_tuple, meta::empty>...>;

            template <typename R, typename... Ts>
            struct ops_t_
            {
                class type;
            };

            template <typename R, typename... Ts>
            using ops_t = typename ops_t_<R, Ts...>::type;

            template <typename R, typename S, std::size_t I, typename... Ts>
            struct recv_t_
            {
                class type;
            };

            template <typename R, typename S, std::size_t I, typename... Ts>
            using recv_t = typename recv_t_<R, S, I, Ts...>::type;

            template <typename R, typename S, std::size_t I, typename... Ts>
            class recv_t_<R, S, I, Ts...>::type
            {
            public:
                explicit type(ops_t<R, Ts...>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<R, Ts...>* ops_;

                env_t<env_of_t<R>> get_env() const noexcept;
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

                template <recvs::completion_cpo SetCpo, typename... Args>
                friend void tag_invoke(SetCpo, type&& self, Args&&... args) noexcept
                {
                    self.ops_->template set<I>(SetCpo{}, static_cast<Args&&>(args)...);
                }
            };

            template <typename... Ts>
            using optional_of_single = std::optional<single_type_t<Ts...>>;

            enum class final_signal
            {
                value,
                error,
                stopped
            };

            template <typename R, typename... Ts, std::size_t... Is>
            auto connect_children(std::index_sequence<Is...>, ops_t<R, Ts...>* ops, Ts&&... snds)
            {
                return ops_tuple<connect_result_t<Ts, recv_t<R, Ts, Is, Ts...>>...>{[&] { //
                    return exec::connect(static_cast<Ts&&>(snds), recv_t<R, Ts, Is, Ts...>(ops));
                }...};
            }

            template <typename R, typename... Ts>
            using children_ops_t = decltype(when_all::connect_children<R>(
                std::index_sequence_for<Ts...>{}, static_cast<ops_t<R, Ts...>*>(nullptr), std::declval<Ts>()...));

            template <typename R, typename... Ts>
            class ops_t_<R, Ts...>::type
            {
            public:
                // clang-format off
                template <typename R2, typename... Us>
                explicit type(R2&& recv, Us&&... snds):
                    recv_(static_cast<R2&&>(recv)),
                    children_(when_all::connect_children<R>(
                        std::index_sequence_for<Ts...>{}, this, static_cast<Us&&>(snds)...)) {}
                // clang-format on

                auto get_recv_env() noexcept
                {
                    return exec::make_env(exec::get_env(recv_), //
                        get_stop_token, stop_src_.get_token());
                }

                template <std::size_t I, typename... Us>
                void set(set_value_t, [[maybe_unused]] Us&&... values) noexcept
                {
                    // If the whole operation is expected to err/stop,
                    // just don't bother storing the result
                    if constexpr (can_send_value<R, Ts...>)
                        // Only do things if no error/stopped signals has been sent
                        // relaxed since dependency is taken care of by the counter
                        if (signal_.load(std::memory_order::relaxed) == final_signal::value)
                        {
                            try
                            {
                                std::get<I>(values_) = std::forward_as_tuple(static_cast<Us&&>(values)...);
                            }
                            catch (...)
                            {
                                set<I>(set_error, std::current_exception());
                                return; // Counter increment taken care of by set(set_error_t, E&&)
                            }
                        }
                    increase_counter(); // Arrives
                }

                template <std::size_t I, typename E>
                void set(set_error_t, E&& error) noexcept
                {
                    // Only do things if we are the first one arriving with a non-value signal
                    if (final_signal expected = final_signal::value;
                        signal_.compare_exchange_strong(expected, final_signal::error, std::memory_order::relaxed))
                    {
                        stop_src_.request_stop(); // Cancel children operations
                        try
                        {
                            error_.template emplace<std::decay_t<E>>(static_cast<E&&>(error));
                        }
                        catch (...)
                        {
                            error_.template emplace<std::exception_ptr>(std::current_exception());
                        }
                    }
                    increase_counter(); // Arrives
                }

                template <std::size_t I>
                void set(set_stopped_t) noexcept
                {
                    // Only do things if we are the first one arriving with a non-value signal
                    if (final_signal expected = final_signal::value;
                        signal_.compare_exchange_strong(expected, final_signal::stopped, std::memory_order::relaxed))
                        stop_src_.request_stop(); // Cancel children operations
                    increase_counter(); // Arrives
                }

            private:
                template <typename S, std::size_t I>
                using recv_for = recv_t<R, S, I, Ts...>;

                struct stop_callback
                {
                    in_place_stop_source& stop_src;
                    void operator()() const noexcept { stop_src.request_stop(); }
                };

                using values_t = typename decltype(
                    []
                    {
                        if constexpr (can_send_value<R, Ts...>)
                            return type_tag<std::tuple<value_types_of_t<Ts, env_of_t<R>, //
                                decayed_tuple, optional_of_single>...>>;
                        else
                            return type_tag<std::monostate>;
                    }())::type;
                using error_t = meta::unpack_invoke< //
                    meta::flatten<error_types_of_t<Ts, env_of_t<R>, type_list>..., type_list<std::exception_ptr>>, //
                    meta::quote<nullable_variant>>;
                using callback_t = typename stop_token_of_t<R>::template callback_type<stop_callback>;

                R recv_;
                children_ops_t<R, Ts...> children_;
                std::atomic_size_t finished_count_{};
                std::atomic<final_signal> signal_{};
                in_place_stop_source stop_src_;
                std::optional<callback_t> callback_;
                CLU_NO_UNIQUE_ADDRESS values_t values_;
                error_t error_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    self.callback_.emplace( // Propagate stop signal
                        exec::get_stop_token(exec::get_env(self.recv_)), //
                        stop_callback{self.stop_src_});
                    if (self.stop_src_.stop_requested()) // Shortcut when the operation is preemptively stopped
                        exec::set_stopped(static_cast<R&&>(self.recv_));
                    // Just start every child operation state
                    apply([](auto&... children) { (exec::start(children), ...); }, self.children_);
                }

                void increase_counter() noexcept
                {
                    if (finished_count_.fetch_add(1, std::memory_order::acq_rel) + 1 == sizeof...(Ts))
                        send_results();
                }

                // The child operations have ended, send the aggregated result to the receiver
                void send_results() noexcept
                {
                    callback_.reset(); // The stop callback won't be needed
                    switch (signal_)
                    {
                        case final_signal::value:
                            if constexpr (can_send_value<R, Ts...>)
                                send_values();
                            else
                                unreachable();
                            return;
                        case final_signal::error:
                            std::visit(
                                [&]<typename E>(E&& error) noexcept
                                {
                                    if constexpr (std::is_same_v<E, monostate>)
                                        unreachable();
                                    else
                                        exec::set_error(static_cast<R&&>(recv_), static_cast<E&&>(error));
                                },
                                std::move(error_));
                            return;
                        case final_signal::stopped: exec::set_stopped(static_cast<R&&>(recv_)); return;
                        default: unreachable();
                    }
                }

                void send_values() noexcept
                {
                    std::apply(
                        [&]<typename... Opts>(Opts&&... opts) noexcept
                        {
                            try
                            {
                                std::apply(
                                    [&]<typename... Us>(Us&&... values) noexcept
                                    {
                                        exec::set_value(static_cast<R&&>(recv_), //
                                            static_cast<Us&&>(values)...);
                                    },
                                    std::tuple_cat(*static_cast<Opts&&>(opts)...));
                            }
                            catch (...)
                            {
                                exec::set_error(static_cast<R&&>(recv_), std::current_exception());
                            }
                        },
                        std::move(values_));
                }
            };

            template <typename R, typename S, std::size_t I, typename... Ts>
            env_t<env_of_t<R>> recv_t_<R, S, I, Ts...>::type::get_env() const noexcept
            {
                return ops_->get_recv_env();
            }

            template <typename... Ts>
            struct snd_t_
            {
                class type;
            };

            template <typename... Ts>
            using snd_t = typename snd_t_<std::decay_t<Ts>...>::type;

            template <typename... Ts>
            class snd_t_<Ts...>::type
            {
            public:
                // clang-format off
                template <typename... Us>
                explicit type(Us&&... snds):
                    snds_(static_cast<Us&&>(snds)...) {}
                // clang-format on

            private:
                std::tuple<Ts...> snds_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return std::apply([&](Ts&&... snds)
                        { return ops_t<R, Ts...>(static_cast<R&&>(recv), static_cast<Ts&&>(snds)...); },
                        std::move(self).snds_);
                }

                template <typename Env>
                constexpr friend auto tag_invoke(get_completion_signatures_t, type&&, Env&&) noexcept
                {
                    if constexpr (meta::all_of_q<> //
                        ::value<value_types_of_t<Ts, env_t<Env>, decayed_tuple, one_or_zero_type>...>)
                    {
                        using non_value_sigs = join_sigs< //
                            make_completion_signatures<Ts, env_t<Env>, completion_signatures<>, //
                                meta::constant_q<completion_signatures<>>::fn>...,
                            completion_signatures<set_error_t(std::exception_ptr), set_stopped_t()>>;
                        // At least one of the senders doesn't send values
                        if constexpr (meta::any_of_q<> //
                            ::value<value_types_of_t<Ts, env_t<Env>, decayed_tuple, meta::empty>...>)
                            return non_value_sigs{};
                        else
                        {
                            using all_values =
                                meta::flatten<value_types_of_t<Ts, env_t<Env>, type_list, single_type_t>...>;
                            using value_sig = meta::unpack_invoke<all_values, meta::quote<sig_of_decayed_rref>>;
                            return add_sig<non_value_sigs, value_sig>{};
                        }
                    }
                    else
                        return dependent_completion_signatures<Env>{};
                }
            };

            struct when_all_t
            {
                template <sender... Ts>
                auto operator()(Ts&&... snds) const
                {
                    if constexpr (tag_invocable<when_all_t, Ts...>)
                    {
                        static_assert(sender<tag_invoke_result_t<when_all_t, Ts...>>,
                            "customization of when_all should return a sender");
                        return clu::tag_invoke(*this, static_cast<Ts&&>(snds)...);
                    }
                    else
                    {
                        if constexpr (sizeof...(Ts) == 0)
                            return just::just_t{}(); // sends nothing
                        else
                            return snd_t<Ts...>(static_cast<Ts&&>(snds)...);
                    }
                }
            };
        } // namespace when_all

        namespace when_any
        {
            template <typename Env>
            using env_t = adapted_env_t<Env, get_stop_token_t, in_place_stop_token>;

            template <typename... Ts>
            using one_or_zero_type = std::bool_constant<(sizeof...(Ts) < 2)>;
            template <typename... Ts>
            using sig_of_decayed_rref = set_value_t(std::decay_t<Ts>&&...);
            template <typename R, typename... Ts>
            inline constexpr bool can_send_value =
                !meta::all_of_q<>::value<value_types_of_t<Ts, env_t<env_of_t<R>>, decayed_tuple, meta::empty>...>;

            template <typename R, typename... Ts>
            struct ops_t_
            {
                class type;
            };

            template <typename R, typename... Ts>
            using ops_t = typename ops_t_<R, Ts...>::type;

            template <typename R, typename S, std::size_t I, typename... Ts>
            struct recv_t_
            {
                class type;
            };

            template <typename R, typename S, std::size_t I, typename... Ts>
            using recv_t = typename recv_t_<R, S, I, Ts...>::type;

            template <typename R, typename S, std::size_t I, typename... Ts>
            class recv_t_<R, S, I, Ts...>::type
            {
            public:
                explicit type(ops_t<R, Ts...>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<R, Ts...>* ops_;

                env_t<env_of_t<R>> get_env() const noexcept;
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

                template <recvs::completion_cpo SetCpo, typename... Args>
                friend void tag_invoke(SetCpo, type&& self, Args&&... args) noexcept
                {
                    self.ops_->template set<I>(SetCpo{}, static_cast<Args&&>(args)...);
                }
            };

            enum class final_signal
            {
                stopped,
                value,
                error
            };

            template <typename R, typename... Ts, std::size_t... Is>
            auto connect_children(std::index_sequence<Is...>, ops_t<R, Ts...>* ops, Ts&&... snds)
            {
                return ops_tuple<connect_result_t<Ts, recv_t<R, Ts, Is, Ts...>>...>{[&] { //
                    return exec::connect(static_cast<Ts&&>(snds), recv_t<R, Ts, Is, Ts...>(ops));
                }...};
            }

            template <typename R, typename... Ts>
            using children_ops_t = decltype(when_any::connect_children<R>(
                std::index_sequence_for<Ts...>{}, static_cast<ops_t<R, Ts...>*>(nullptr), std::declval<Ts>()...));

            template <typename... Ts>
            struct fuck;

            template <typename R, typename... Ts>
            class ops_t_<R, Ts...>::type
            {
            public:
                // clang-format off
                template <typename R2, typename... Us>
                explicit type(R2&& recv, Us&&... snds):
                    recv_(static_cast<R2&&>(recv)),
                    children_(when_any::connect_children<R>(
                        std::index_sequence_for<Ts...>{}, this, static_cast<Us&&>(snds)...)) {}
                // clang-format on

                auto get_recv_env() noexcept
                {
                    return exec::make_env(exec::get_env(recv_), //
                        get_stop_token, stop_src_.get_token());
                }

                template <std::size_t I, typename... Us>
                void set(set_value_t, [[maybe_unused]] Us&&... values) noexcept
                {
                    // We're the first to complete with a value
                    // relaxed since dependency is taken care of by the counter
                    if (signal_.exchange(final_signal::value, std::memory_order::relaxed) != final_signal::value)
                    {
                        stop_src_.request_stop(); // We succeeded, cancel all other child tasks
                        try
                        {
                            using tuple = decayed_tuple<Us...>;
                            values_.template emplace<tuple>(static_cast<Us&&>(values)...);
                        }
                        catch (...)
                        {
                            set<I>(set_error, std::current_exception());
                            return; // Counter increment taken care of by set(set_error_t, E&&)
                        }
                    }
                    increase_counter(); // Arrives
                }

                template <std::size_t I, typename E>
                void set(set_error_t, E&& error) noexcept
                {
                    // If there is already a value signal, our results are not useful
                    // Only set the state to error if it is currently the stopped (default) state
                    if (final_signal expected = final_signal::stopped;
                        signal_.compare_exchange_strong(expected, final_signal::error, std::memory_order::relaxed))
                    {
                        try
                        {
                            error_.template emplace<std::decay_t<E>>(static_cast<E&&>(error));
                        }
                        catch (...)
                        {
                            error_.template emplace<std::exception_ptr>(std::current_exception());
                        }
                    }
                    increase_counter(); // Arrives
                }

                template <std::size_t I>
                void set(set_stopped_t) noexcept
                {
                    increase_counter(); // Arrives, no other action is needed
                }

            private:
                template <typename S, std::size_t I>
                using recv_for = recv_t<R, S, I, Ts...>;

                struct stop_callback
                {
                    in_place_stop_source& stop_src;
                    void operator()() const noexcept { stop_src.request_stop(); }
                };

                using values_t = typename decltype(
                    []
                    {
                        if constexpr (can_send_value<R, Ts...>)
                        {
                            using flattened =
                                meta::flatten<value_types_of_t<Ts, env_of_t<R>, decayed_tuple, type_list>...>;
                            return type_tag<meta::unpack_invoke<flattened, meta::quote<nullable_variant>>>;
                        }
                        else
                            return type_tag<std::monostate>;
                    }())::type;
                using error_t = meta::unpack_invoke< //
                    meta::flatten<error_types_of_t<Ts, env_of_t<R>, type_list>..., type_list<std::exception_ptr>>, //
                    meta::quote<nullable_variant>>;
                using callback_t = typename stop_token_of_t<R>::template callback_type<stop_callback>;

                R recv_;
                children_ops_t<R, Ts...> children_;
                std::atomic_size_t finished_count_{};
                std::atomic<final_signal> signal_{};
                in_place_stop_source stop_src_;
                std::optional<callback_t> callback_;
                CLU_NO_UNIQUE_ADDRESS values_t values_;
                error_t error_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    self.callback_.emplace( // Propagate stop signal
                        exec::get_stop_token(exec::get_env(self.recv_)), //
                        stop_callback{self.stop_src_});
                    if (self.stop_src_.stop_requested()) // Shortcut when the operation is preemptively stopped
                        exec::set_stopped(static_cast<R&&>(self.recv_));
                    // Just start every child operation state
                    apply([](auto&... children) { (exec::start(children), ...); }, self.children_);
                }

                void increase_counter() noexcept
                {
                    if (finished_count_.fetch_add(1, std::memory_order::acq_rel) + 1 == sizeof...(Ts))
                        send_results();
                }

                // The child operations have ended, send the aggregated result to the receiver
                void send_results() noexcept
                {
                    callback_.reset(); // The stop callback won't be needed
                    switch (signal_)
                    {
                        case final_signal::value:
                            if constexpr (can_send_value<R, Ts...>)
                                send_values();
                            else
                                unreachable();
                            return;
                        case final_signal::error:
                            std::visit(
                                [&]<typename E>(E&& error) noexcept
                                {
                                    if constexpr (std::is_same_v<E, monostate>)
                                        unreachable();
                                    else
                                        exec::set_error(static_cast<R&&>(recv_), static_cast<E&&>(error));
                                },
                                std::move(error_));
                            return;
                        case final_signal::stopped: exec::set_stopped(static_cast<R&&>(recv_)); return;
                        default: unreachable();
                    }
                }

                void send_values() noexcept
                {
                    std::visit(
                        [&]<typename Tup>(Tup&& tuple) noexcept -> void
                        {
                            if constexpr (std::is_same_v<Tup, std::monostate>)
                                unreachable();
                            else
                                std::apply(
                                    [&]<typename... Us>(Us&&... values) noexcept
                                    {
                                        exec::set_value(static_cast<R&&>(recv_), //
                                            static_cast<Us&&>(values)...);
                                    },
                                    static_cast<Tup&&>(tuple));
                        },
                        std::move(values_));
                }
            };

            template <typename R, typename S, std::size_t I, typename... Ts>
            env_t<env_of_t<R>> recv_t_<R, S, I, Ts...>::type::get_env() const noexcept
            {
                return ops_->get_recv_env();
            }

            template <typename... Ts>
            struct snd_t_
            {
                class type;
            };

            template <typename... Ts>
            using snd_t = typename snd_t_<std::decay_t<Ts>...>::type;

            template <typename... Ts>
            class snd_t_<Ts...>::type
            {
            public:
                // clang-format off
                template <typename... Us>
                explicit type(Us&&... snds):
                    snds_(static_cast<Us&&>(snds)...) {}
                // clang-format on

            private:
                std::tuple<Ts...> snds_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return std::apply([&](Ts&&... snds)
                        { return ops_t<R, Ts...>(static_cast<R&&>(recv), static_cast<Ts&&>(snds)...); },
                        std::move(self).snds_);
                }

                template <typename Env>
                constexpr friend auto tag_invoke(get_completion_signatures_t, type&&, Env&&) noexcept
                {
                    if constexpr (meta::all_of_q<> //
                        ::value<value_types_of_t<Ts, env_t<Env>, decayed_tuple, one_or_zero_type>...>)
                    {
                        using non_value_sigs = join_sigs< //
                            make_completion_signatures<Ts, env_t<Env>, completion_signatures<>, //
                                meta::constant_q<completion_signatures<>>::fn>...,
                            completion_signatures<set_error_t(std::exception_ptr), set_stopped_t()>>;
                        // All of the senders doesn't send values
                        if constexpr (meta::all_of_q<> //
                            ::value<value_types_of_t<Ts, env_t<Env>, decayed_tuple, meta::empty>...>)
                            return non_value_sigs{};
                        else
                            return join_sigs<
                                value_types_of_t<Ts, env_t<Env>, sig_of_decayed_rref, completion_signatures>...,
                                non_value_sigs>{};
                    }
                    else
                        return dependent_completion_signatures<Env>{};
                }
            };

            struct when_any_t
            {
                template <sender... Ts>
                auto operator()(Ts&&... snds) const
                {
                    if constexpr (tag_invocable<when_any_t, Ts...>)
                    {
                        static_assert(sender<tag_invoke_result_t<when_any_t, Ts...>>,
                            "customization of when_any should return a sender");
                        return clu::tag_invoke(*this, static_cast<Ts&&>(snds)...);
                    }
                    else
                    {
                        if constexpr (sizeof...(Ts) == 0)
                            return just::just_t{}(); // sends nothing
                        else
                            return snd_t<Ts...>(static_cast<Ts&&>(snds)...);
                    }
                }
            };
        } // namespace when_any

        namespace into_var
        {
            template <typename S, typename Env>
            using variant_of = value_types_of_t<S, Env>;

            template <typename T, bool WithSetValue = false>
            struct comp_sigs_of_construction
            {
                template <typename... Ts>
                using fn = filtered_sigs<
                    conditional_t<std::is_nothrow_constructible_v<T, Ts...>, void, set_error_t(std::exception_ptr)>,
                    conditional_t<WithSetValue, set_value_t(T), void>>;
            };

            template <typename S, typename Env>
            using snd_comp_sigs =
                make_completion_signatures<S, Env, completion_signatures<set_value_t(variant_of<S, Env>)>,
                    comp_sigs_of_construction<variant_of<S, Env>>::template fn>;

            template <typename R, typename Var>
            struct recv_t_
            {
                struct type;
            };

            template <typename R, typename Var>
            using recv_t = typename recv_t_<std::remove_cvref_t<R>, Var>::type;

            template <typename R, typename Var>
            struct recv_t_<R, Var>::type : receiver_adaptor<recv_t<R, Var>, R>
            {
                using receiver_adaptor<recv_t<R, Var>, R>::receiver_adaptor;

                template <typename... Ts>
                using comp_sigs = typename comp_sigs_of_construction<decayed_tuple<Ts...>, true>::template fn<Ts...>;

                template <typename... Ts>
                    requires receiver_of<R, comp_sigs<Ts...>>
                void set_value(Ts&&... args) && noexcept
                {
                    using tuple = decayed_tuple<Ts...>;
                    if constexpr (std::is_nothrow_constructible_v<tuple, Ts...>)
                    {
                        exec::set_value(
                            std::move(*this).base(), Var(std::in_place_type<tuple>, static_cast<Ts&&>(args)...));
                    }
                    else
                    {
                        try
                        {
                            exec::set_value(
                                std::move(*this).base(), Var(std::in_place_type<tuple>, static_cast<Ts&&>(args)...));
                        }
                        catch (...)
                        {
                            exec::set_error(std::move(*this).base(), std::current_exception());
                        }
                    }
                }
            };

            template <typename S>
            struct snd_t_
            {
                class type;
            };

            template <typename S>
            class snd_t_<S>::type
            {
            public:
                // clang-format off
                template <typename S2>
                constexpr explicit type(S2&& snd) noexcept(std::is_nothrow_constructible_v<S, S2>):
                    snd_(static_cast<S2&&>(snd)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;

                template <typename R>
                constexpr friend auto tag_invoke(connect_t, type&& snd, R&& recv)
                {
                    return exec::connect(
                        static_cast<type&&>(snd).snd_, recv_t<R, variant_of<S, env_of_t<R>>>(static_cast<R&&>(recv)));
                }

                template <snd_qry::fwd_snd_query Q, typename... Args>
                    requires callable<Q, const S&, Args...>
                constexpr friend auto tag_invoke(Q, const type& snd, Args&&... args) noexcept(
                    nothrow_callable<Q, const type&, Args...>)
                {
                    return Q{}(snd.snd_, static_cast<Args&&>(args)...);
                }

                template <typename Env>
                constexpr friend snd_comp_sigs<S, Env> tag_invoke(get_completion_signatures_t, const type&, Env)
                {
                    return {};
                }
            };

            template <typename S>
            using snd_t = typename snd_t_<std::remove_cvref_t<S>>::type;

            struct into_variant_t
            {
                template <sender S>
                auto operator()(S&& snd) const
                {
                    return snd_t<S>(static_cast<S&&>(snd));
                }
                constexpr auto operator()() const noexcept { return make_piper(*this); }
            };
        } // namespace into_var

        namespace start_det
        {
            template <typename S>
            struct ops_wrapper;

            template <typename S>
            struct recv_t_
            {
                struct type;
            };

            template <typename S>
            struct recv_t_<S>::type
            {
                ops_wrapper<S>* ptr = nullptr;

                template <typename... Ts>
                friend void tag_invoke(set_value_t, type&& self, Ts&&...) noexcept
                {
                    delete self.ptr;
                }

                template <typename = int>
                friend void tag_invoke(set_stopped_t, type&& self) noexcept
                {
                    delete self.ptr;
                }

                template <typename E>
                friend void tag_invoke(set_error_t, type&& self, E&&) noexcept
                {
                    delete self.ptr;
                    std::terminate();
                }

                friend empty_env tag_invoke(get_env_t, const type&) noexcept { return {}; }
            };

            template <typename S>
            using recv_t = typename recv_t_<S>::type;

            template <typename S>
            struct ops_wrapper
            {
                connect_result_t<S, recv_t<S>> op_state;
                explicit ops_wrapper(S&& snd): op_state(exec::connect(static_cast<S&&>(snd), recv_t<S>{.ptr = this})) {}
            };

            struct start_detached_t
            {
                template <sender S>
                constexpr void operator()(S&& snd) const
                {
                    if constexpr (requires {
                                      requires tag_invocable<start_detached_t,
                                          call_result_t<get_completion_scheduler_t<set_value_t>, S>, S>;
                                  })
                    {
                        static_assert(std::is_void_v<tag_invoke_result_t<start_detached_t,
                                          call_result_t<get_completion_scheduler_t<set_value_t>, S>, S>>,
                            "start_detached should return void");
                        clu::tag_invoke(*this, exec::get_completion_scheduler<set_value_t>(snd), static_cast<S&&>(snd));
                    }
                    else if constexpr (tag_invocable<start_detached_t, S>)
                    {
                        static_assert(std::is_void_v<tag_invoke_result_t<start_detached_t, S>>,
                            "start_detached should return void");
                        clu::tag_invoke(*this, static_cast<S&&>(snd));
                    }
                    else
                        exec::start((new ops_wrapper<S>(static_cast<S&&>(snd)))->op_state);
                }
            };
        } // namespace start_det

        namespace execute
        {
            struct execute_t
            {
                template <scheduler S, std::invocable F>
                void operator()(S&& schd, F&& func) const
                {
                    if constexpr (tag_invocable<execute_t, S, F>)
                    {
                        static_assert(std::is_void_v<tag_invoke_result_t<execute_t, S, F>>,
                            "customization for execute should return void");
                        clu::tag_invoke(*this, static_cast<S&&>(schd), static_cast<F&&>(func));
                    }
                    else
                    {
                        start_det::start_detached_t{}(
                            upon::then_t{}(exec::schedule(static_cast<S&&>(schd)), static_cast<F&&>(func)));
                    }
                }
            };
        } // namespace execute
    } // namespace detail

    using detail::just::just_t;
    using detail::just::just_error_t;
    using detail::just::just_stopped_t;
    using detail::stop_req::stop_if_requested_t;
    using detail::upon::then_t;
    using detail::upon::upon_error_t;
    using detail::upon::upon_stopped_t;
    using detail::let::let_value_t;
    using detail::let::let_error_t;
    using detail::let::let_stopped_t;
    using detail::schd_from::schedule_from_t;
    using detail::on::on_t;
    using detail::xfer::transfer_t;
    using detail::xfer::transfer_just_t;
    using detail::when_all::when_all_t;
    using detail::when_any::when_any_t;
    using detail::into_var::into_variant_t;
    using detail::start_det::start_detached_t;
    using detail::execute::execute_t;

    inline constexpr just_t just{};
    inline constexpr just_error_t just_error{};
    inline constexpr just_stopped_t just_stopped{};
    inline constexpr stop_if_requested_t stop_if_requested{};
    inline constexpr just_stopped_t stop{};
    inline constexpr then_t then{};
    inline constexpr upon_error_t upon_error{};
    inline constexpr upon_stopped_t upon_stopped{};
    inline constexpr let_value_t let_value{};
    inline constexpr let_error_t let_error{};
    inline constexpr let_stopped_t let_stopped{};
    inline constexpr on_t on{};
    inline constexpr schedule_from_t schedule_from{};
    inline constexpr transfer_t transfer{};
    inline constexpr transfer_just_t transfer_just{};
    inline constexpr when_all_t when_all{};
    inline constexpr when_any_t when_any{};
    inline constexpr into_variant_t into_variant{};
    inline constexpr start_detached_t start_detached{};
    inline constexpr execute_t execute{};

    inline struct just_from_t
    {
        template <std::invocable F>
        constexpr auto operator()(F&& func) const
        {
            return just() | then(static_cast<F&&>(func));
        }
    } constexpr just_from{};

    inline struct defer_t
    {
        template <std::invocable F>
            requires sender<std::invoke_result_t<F>>
        constexpr auto operator()(F&& func) const
        {
            // clang-format off
            return just()
                | let_value(static_cast<F&&>(func));
            // clang-format on
        }
    } constexpr defer{};

    inline struct stopped_as_optional_t
    {
        template <sender S>
        auto operator()(S&& snd) const
        {
            using detail::coro_utils::single_sender;
            using detail::coro_utils::single_sender_value_type;
            return read(std::identity{}) |
                let_value(
                    // clang-format off
                    [snd = static_cast<S&&>(snd)]<class E>
                        requires single_sender<S, E>
                        (const E&) mutable
                    {
                        using opt = std::optional<std::decay_t<single_sender_value_type<S, E>>>;
                        return static_cast<S&&>(snd)
                            | then([]<class T>(T&& t) { return opt{static_cast<T&&>(t)}; })
                            | let_stopped([]() noexcept { return just(opt{}); });
                    }
                    // clang-format on
                );
        }

        constexpr auto operator()() const noexcept { return make_piper(*this); }
    } constexpr stopped_as_optional{};

    inline struct stopped_as_error_t
    {
        template <sender S, movable_value E>
        auto operator()(S&& snd, E&& err) const
        {
            return let_stopped(static_cast<S&&>(snd),
                [err = static_cast<E&&>(err)]() mutable { return just_error(static_cast<E&&>(err)); });
        }

        template <movable_value E>
        constexpr auto operator()(E&& err) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<E&&>(err)));
        }
    } constexpr stopped_as_error{};

    inline struct materialize_t
    {
        template <sender S>
        auto operator()(S&& snd) const
        {
            return static_cast<S&&>(snd) //
                | let_value([]<typename... Ts>(Ts&&... vs) { return just(set_value, static_cast<Ts&&>(vs)...); }) //
                | let_error([]<typename E>(E&& err) { return just(set_error, static_cast<E&&>(err)); }) //
                | let_stopped([] { return just(set_stopped); });
        }

        constexpr auto operator()() const noexcept { return make_piper(*this); }
    } constexpr materialize{};

    inline struct dematerialize_t
    {
        template <sender S>
        auto operator()(S&& snd) const
        {
            return let_value(static_cast<S&&>(snd), //
                []<typename Cpo, typename... Ts>(Cpo, Ts&&... vs)
                {
                    if constexpr (std::is_same_v<Cpo, set_value_t>)
                        return just(static_cast<Ts&&>(vs)...);
                    else if constexpr (std::is_same_v<Cpo, set_error_t>)
                        return just_error(static_cast<Ts&&>(vs)...);
                    else if constexpr (std::is_same_v<Cpo, set_stopped_t>)
                        return just_stopped();
                    else
                        static_assert(dependent_false<S>);
                });
        }

        constexpr auto operator()() const noexcept { return make_piper(*this); }
    } constexpr dematerialize{};

    inline struct into_tuple_t
    {
        template <sender S>
        auto operator()(S&& snd) const
        {
            // clang-format off
            return then(
                static_cast<S&&>(snd),
                []<typename... Ts>(Ts&&... args) noexcept(
                    (std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts> && ...))
                { return detail::decayed_tuple<Ts...>(static_cast<Ts&&>(args)...); }
            );
            // clang-format on
        }

        constexpr auto operator()() const noexcept { return make_piper(*this); }
    } constexpr into_tuple{};
} // namespace clu::exec

namespace clu::this_thread
{
    namespace detail::sync_wait
    {
        template <typename S>
        struct recv_
        {
            class type;
        };

        using loop_schd = decltype(std::declval<run_loop&>().get_scheduler());

        struct env_t
        {
            loop_schd schd;
            friend loop_schd tag_invoke(exec::get_scheduler_t, const env_t& self) noexcept { return self.schd; }
            friend loop_schd tag_invoke(exec::get_delegatee_scheduler_t, const env_t& self) noexcept
            {
                return self.schd;
            }
        };

        template <typename S>
        using value_types_t = exec::value_types_of_t<S, env_t, exec::detail::decayed_tuple, single_type_t>;
        template <typename S>
        using result_t = std::optional<value_types_t<S>>;
        template <typename S>
        using variant_t = std::variant<std::monostate, value_types_t<S>, std::exception_ptr, exec::set_stopped_t>;

        // clang-format off
        template <typename S>
        concept sync_waitable_sender =
            exec::sender<S, env_t> && 
            requires { typename result_t<S>; };
        // clang-format on

        template <typename S>
        class recv_<S>::type
        {
        public:
            type(run_loop* loop, variant_t<S>* ptr): loop_(loop), ptr_(ptr) {}

        private:
            run_loop* loop_;
            variant_t<S>* ptr_;

            friend env_t tag_invoke(exec::get_env_t, const type& self) noexcept
            {
                return {self.loop_->get_scheduler()};
            }

            template <typename... Ts>
                requires std::constructible_from<value_types_t<S>, Ts...>
            friend void tag_invoke(exec::set_value_t, type&& self, Ts&&... args) noexcept
            {
                try
                {
                    self.ptr_->template emplace<1>(static_cast<Ts&&>(args)...);
                    self.loop_->finish();
                }
                catch (...)
                {
                    exec::set_error(std::move(self), std::current_exception());
                }
            }

            template <typename E>
            friend void tag_invoke(exec::set_error_t, type&& self, E&& error) noexcept
            {
                self.ptr_->template emplace<2>(exec::detail::make_exception_ptr(static_cast<E&&>(error)));
                self.loop_->finish();
            }

            friend void tag_invoke(exec::set_stopped_t, type&& self) noexcept
            {
                self.ptr_->template emplace<3>();
                self.loop_->finish();
            }
        };

        template <typename S>
        using recv_t = typename recv_<std::remove_cvref_t<S>>::type;

        struct sync_wait_t
        {
            template <sync_waitable_sender S>
            result_t<S> operator()(S&& snd) const
            {
                if constexpr (requires {
                                  requires tag_invocable<sync_wait_t,
                                      exec::detail::completion_scheduler_of_t<exec::set_value_t, S>, S>;
                              })
                {
                    using comp_schd = exec::detail::completion_scheduler_of_t<exec::set_value_t, S>;
                    static_assert(std::is_same_v<tag_invoke_result_t<sync_wait_t, comp_schd, S>, result_t<S>>);
                    return clu::tag_invoke(*this,
                        exec::get_completion_scheduler<exec::set_value_t>(static_cast<S&&>(snd)),
                        static_cast<S&&>(snd));
                }
                else if constexpr (tag_invocable<sync_wait_t, S>)
                {
                    static_assert(std::is_same_v<tag_invoke_result_t<sync_wait_t, S>, result_t<S>>);
                    return clu::tag_invoke(*this, static_cast<S&&>(snd));
                }
                else
                {
                    run_loop ctx;
                    variant_t<S> result;
                    auto ops = exec::connect(static_cast<S&&>(snd), recv_t<S>(&ctx, &result));
                    exec::start(ops);
                    ctx.run();
                    switch (result.index())
                    {
                        case 1: return std::get<1>(std::move(result));
                        case 2: std::rethrow_exception(std::get<2>(std::move(result)));
                        case 3: return std::nullopt;
                        default: unreachable();
                    }
                }
            }
        };

        template <typename S>
        using into_var_snd_t = call_result_t<exec::into_variant_t, S>;

        template <typename S>
        using var_result_t = result_t<into_var_snd_t<S>>;

        struct sync_wait_with_variant_t
        {
            template <typename S>
                requires sync_waitable_sender<into_var_snd_t<S>>
            constexpr var_result_t<S> operator()(S&& snd) const
            {
                if constexpr (requires {
                                  requires tag_invocable<sync_wait_with_variant_t,
                                      exec::detail::completion_scheduler_of_t<exec::set_value_t, S>, S>;
                              })
                {
                    using comp_schd = exec::detail::completion_scheduler_of_t<exec::set_value_t, S>;
                    static_assert(
                        std::is_same_v<tag_invoke_result_t<sync_wait_with_variant_t, comp_schd, S>, var_result_t<S>>);
                    return clu::tag_invoke(*this,
                        exec::get_completion_scheduler<exec::set_value_t>(static_cast<S&&>(snd)),
                        static_cast<S&&>(snd));
                }
                else if constexpr (tag_invocable<sync_wait_with_variant_t, S>)
                {
                    static_assert(std::is_same_v<tag_invoke_result_t<sync_wait_with_variant_t, S>, var_result_t<S>>);
                    return clu::tag_invoke(*this, static_cast<S&&>(snd));
                }
                else
                    return sync_wait_t{}(exec::into_variant(static_cast<S&&>(snd)));
            }
        };
    } // namespace detail::sync_wait

    using detail::sync_wait::sync_wait_t;
    using detail::sync_wait::sync_wait_with_variant_t;

    inline constexpr sync_wait_t sync_wait{};
    inline constexpr sync_wait_with_variant_t sync_wait_with_variant{};
} // namespace clu::this_thread
