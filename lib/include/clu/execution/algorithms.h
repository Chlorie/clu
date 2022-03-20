#pragma once

#include <optional>

#include "execution_traits.h"
#include "utility.h"
#include "contexts.h"
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
                return clu::tag_invoke(Cpo{}, exec::get_completion_scheduler<SetCpo>(snd), static_cast<S&&>(snd),
                    static_cast<Args&&>(args)...);
            }
            else // if constexpr (tag_invocable<Cpo, S, Args...>)
            {
                static_assert(sender<tag_invoke_result_t<Cpo, S, Args...>>,
                    "customizations for sender algorithms should return senders");
                return clu::tag_invoke(Cpo{}, static_cast<S&&>(snd), static_cast<Args&&>(args)...);
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
                [[no_unique_address]] R recv_;
                [[no_unique_address]] std::tuple<Ts...> values_;

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
                [[no_unique_address]] std::tuple<Ts...> values_;

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
                template <typename R2>
                type(R2&& recv, F&& func) noexcept(
                    std::is_nothrow_constructible_v<R, R2> &&
                    std::is_nothrow_move_constructible_v<F>):
                    recv_(static_cast<R2&&>(recv)),
                    func_(static_cast<F&&>(func)) {}
                // clang-format on

            private:
                [[no_unique_address]] R recv_;
                [[no_unique_address]] F func_;

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
                [[no_unique_address]] S snd_;
                [[no_unique_address]] F func_;

                template <typename R>
                constexpr friend auto tag_invoke(connect_t, type&& snd, R&& recv) noexcept(
                    conn::nothrow_connectable<S, recv_t<R, Cpo, F>>)
                {
                    return exec::connect(static_cast<type&&>(snd).snd_,
                        recv_t<R, Cpo, F>(static_cast<R&&>(recv), static_cast<F&&>(snd.func_)));
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

            template <typename... Ts>
            using nullable_variant =
                meta::unpack_invoke<meta::unique<std::monostate, Ts...>, meta::quote<std::variant>>;

            template <typename S, typename R, typename Cpo, typename F>
            struct storage;

            template <typename S, typename R, typename F>
            struct storage<S, R, set_value_t, F>
            {
                template <typename... Args>
                using ops_for = connect_result_t<std::invoke_result_t<F, decay_lref<Args>...>, R>;

                value_types_of_t<S, env_of_t<R>, decayed_tuple, nullable_variant> args;
                value_types_of_t<S, env_of_t<R>, ops_for, nullable_variant> ops;
            };

            template <typename S, typename R, typename F>
            struct storage<S, R, set_error_t, F>
            {
                template <typename... Es>
                using args_variant_for = nullable_variant<decayed_tuple<Es>...>;
                template <typename... Es>
                using ops_variant_for =
                    nullable_variant<connect_result_t<std::invoke_result_t<F, decay_lref<Es>>, R>...>;

                error_types_of_t<S, env_of_t<R>, args_variant_for> args;
                error_types_of_t<S, env_of_t<R>, ops_variant_for> ops;
            };

            template <typename S, typename R, typename F>
            struct storage<S, R, set_stopped_t, F>
            {
                std::variant<std::tuple<>> args;
                std::variant<std::monostate, connect_result_t<std::invoke_result_t<F>, R>> ops;
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
                template <typename R2>
                type(S&& snd, R2&& recv, F&& func) noexcept(
                    conn::nothrow_connectable<S, recv_t<S, R, Cpo, F>> &&
                    std::is_nothrow_constructible_v<R, R2&&> &&
                    std::is_nothrow_move_constructible_v<F>):
                    initial_ops_(exec::connect(static_cast<S&&>(snd), recv_t<S, R, Cpo, F>(this))),
                    recv_(static_cast<R2&&>(recv)), func_(static_cast<F&&>(func)) {}
                // clang-format on

            private:
                friend recv_t<S, R, Cpo, F>;

                connect_result_t<S, recv_t<S, R, Cpo, F>> initial_ops_;
                R recv_;
                [[no_unique_address]] F func_;
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
                [[no_unique_address]] S snd_;
                [[no_unique_address]] F func_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv) CLU_SINGLE_RETURN(ops_t<S, R, Cpo, F>(
                    static_cast<S&&>(self.snd_), static_cast<R&&>(recv), static_cast<F&&>(self.func_)));

                // clang-format off
                template <typename Env>
                constexpr friend make_completion_signatures<S, Env,
                    completion_signatures<set_error_t(std::exception_ptr)>,
                    make_sig_impl<Cpo, Env, F>::template value_sig,
                    make_sig_impl<Cpo, Env, F>::template error_sig,
                    typename make_sig_impl<Cpo, Env, F>::stopped_sig>
                // clang-format on
                tag_invoke(get_completion_signatures_t, type&&, Env) noexcept
                {
                    return {};
                }
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
            template <typename Env, typename S>
            struct replace_schd_env_
            {
                class type;
            };

            template <typename Env, typename S>
            class replace_schd_env_<Env, S>::type
            {
            public:
                template <typename Env2, typename S2>
                type(Env2&& env, S2&& schd) noexcept: base_(static_cast<Env2&&>(env)), schd_(static_cast<S2&&>(schd))
                {
                }

            private:
                [[no_unique_address]] Env base_;
                [[no_unique_address]] S schd_;

                template <exec_envs::fwd_env_query Cpo, typename... Ts>
                    requires callable<Cpo, const type&, Ts...>
                friend decltype(auto) tag_invoke(Cpo, const type& self, Ts&&... args) noexcept(
                    nothrow_callable<Cpo, const type&, Ts...>)
                {
                    return Cpo{}(self.base_, static_cast<Ts&&>(args)...);
                }

                friend S tag_invoke(get_scheduler_t, const type& self) noexcept { return self.schd_; }
            };

            template <typename Env, typename S>
            auto replace_schd(Env&& env, S&& schd) noexcept
            {
                using env_t = typename replace_schd_env_<std::remove_cvref_t<Env>, std::remove_cvref_t<S>>::type;
                return env_t(static_cast<Env&&>(env), static_cast<S&&>(schd));
            }

            template <typename Schd, typename Snd>
            struct snd_t_
            {
                class type;
            };

            template <typename Schd, typename Snd>
            using snd_t = typename snd_t_<Schd, Snd>::type;

            template <typename Schd, typename Snd>
            class snd_t_<Schd, Snd>::type
            {
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
                    {
                        // TODO: default impl
                    }
                }
            };
        } // namespace on

        namespace schd_from
        {
            template <typename Cpo, typename... Ts>
            std::tuple<Cpo, std::decay_t<Ts>...> storage_tuple_impl(Cpo (*)(Ts...));
            template <typename Sig>
            using storage_tuple_t = decltype(schd_from::storage_tuple_impl(static_cast<Sig*>(nullptr)));

            template <typename Sigs>
            class signal_storage
            {
            public:
                template <typename Cpo, typename... Ts>
                void emplace(Cpo, Ts&&... values)
                {
                    using tuple_t = storage_tuple_t<Cpo(Ts...)>;
                    var_.template emplace<tuple_t>(Cpo{}, static_cast<Ts&&>(values)...);
                }

                template <typename R>
                void pass_to_receiver(R&& recv) noexcept
                {
                    std::visit(
                        [&]<typename Tup>(Tup&& tuple) noexcept
                        {
                            // clang-format off
                            if constexpr (similar_to<Tup, std::monostate>)
                                unreachable();
                            else
                                std::apply([&]<typename Cpo, typename... Ts>(Cpo, Ts&&... values) noexcept
                                {
                                    Cpo{}(static_cast<R&&>(recv), static_cast<Ts&&>(values)...);
                                }, static_cast<Tup&&>(tuple));
                            // clang-format on
                        },
                        static_cast<variant_t&&>(var_));
                }

            private:
                using variant_t = meta::unpack_invoke<
                    meta::push_back_l<meta::transform_l<Sigs, meta::quote1<storage_tuple_t>>, std::monostate>,
                    meta::quote<std::variant>>;

                variant_t var_{};
            };

            template <typename R>
            struct recv_t_
            {
                class type;
            };

            template <typename R>
            using recv_t = typename recv_t_<R>::type;

            template <typename R>
            class recv_t_<R>::type : public receiver_adaptor<type, R>
            {
            public:
                using receiver_adaptor<type, R>::receiver_adaptor;

            private:
            };

            template <typename Schd, typename Snd>
            struct snd_t_
            {
                class type;
            };

            template <typename Schd, typename Snd>
            using snd_t = typename snd_t_<Schd, Snd>::type;

            // constructs a sender s2
            template <typename Schd, typename Snd>
            class snd_t_<Schd, Snd>::type
            {
            public:

            private:
                // when s2 connected to out_r
                template <typename R>
                auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    (void)self;
                    (void)recv;
                    // ops1 contains ops2 and ops3, also what s sends
                    // ops2 = connect(s, r)
                    // ops3 = connect(s3, r2)

                    // Constructs a receiver r such that
                    // when a receiver completion-signal Signal(r, args...) is called,
                    // it decay-copies args... into op_state (see below) as args'...
                    // and constructs a receiver r2 such that:
                    //     When execution::set_value(r2) is called, it calls Signal(out_r, std::move(args')...).
                    //     execution::set_error(r2, e) is expression-equivalent to execution::set_error(out_r, e).
                    //     execution::set_stopped(r2) is expression-equivalent to execution::set_stopped(out_r).
                    // It then calls execution::schedule(sch), resulting in a sender s3.
                    // It then calls execution::connect(s3, r2), resulting in an operation state op_state3.
                    // It then calls execution::start(op_state3).
                    // If any of these throws an exception, it catches it
                    // and calls execution::set_error(out_r, current_exception()).
                    // If any of these expressions would be ill-formed, then Signal(r, args...) is ill-formed.

                    // Calls execution::connect(s, r) resulting in an operation state op_state2.
                    // If this expression would be ill-formed, execution::connect(s2, out_r) is ill-formed.

                    // Returns an operation state op_state that contains op_state2.
                    // When execution::start(op_state) is called, calls execution::start(op_state2).
                    // The lifetime of op_state3 ends when op_state is destroyed.
                }
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
                    {
                        // TODO: default impl
                    }
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
                auto operator()(Schd&& schd) const noexcept
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<Schd&&>(schd)));
                }
            };
        } // namespace xfer

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
                [[no_unique_address]] S snd_;

                template <typename R>
                constexpr friend auto tag_invoke(connect_t, type&& snd, R&& recv)
                {
                    return exec::connect(
                        static_cast<type&&>(snd).snd_, recv_t<R, variant_of<S, env_of_t<R>>>(static_cast<R&&>(recv)));
                }

                template <typename Env>
                constexpr friend snd_comp_sigs<S, Env> tag_invoke(get_completion_signatures_t, type&&, Env)
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

        namespace stopped_as
        {
            class stopped_as_optional_t
            {
                template <sender S>
                auto operator()(S&& snd) const
                {
                    using coro_utils::single_sender;
                    using coro_utils::single_sender_value_type;
                    return exec::read(std::identity{}) |
                        let::let_value_t{}(
                            // clang-format off
                            [snd = static_cast<S&&>(snd)]<class E>
                                requires single_sender<S, E>
                                (const E&) mutable
                            {
                                using opt = std::optional<std::decay_t<single_sender_value_type<S, E>>>;
                                return static_cast<S&&>(snd)
                                    | upon::then_t{}([]<class T>(T&& t) { return opt{static_cast<T&&>(t)}; })
                                    | let::let_stopped_t{}([]() noexcept { return just::just_t{}(opt{}); });
                            }
                            // clang-format on
                        );
                }

                constexpr auto operator()() const noexcept { return make_piper(*this); }
            };

            class stopped_as_error_t
            {
                template <sender S, movable_value E>
                auto operator()(S&& snd, E&& err) const
                {
                    return let::let_stopped_t{}(static_cast<S&&>(snd),
                        [err = static_cast<E&&>(err)]() mutable
                        { return just::just_error_t{}(static_cast<E&&>(err)); });
                }

                template <movable_value E>
                constexpr auto operator()(E&& err) const
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<E&&>(err)));
                }
            };
        } // namespace stopped_as

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
    inline constexpr just_t just{};
    inline constexpr just_error_t just_error{};
    inline constexpr just_stopped_t just_stopped{};
    inline constexpr auto stop = just_stopped();
    using detail::upon::then_t;
    using detail::upon::upon_error_t;
    using detail::upon::upon_stopped_t;
    inline constexpr then_t then{};
    inline constexpr upon_error_t upon_error{};
    inline constexpr upon_stopped_t upon_stopped{};
    using detail::let::let_value_t;
    using detail::let::let_error_t;
    using detail::let::let_stopped_t;
    inline constexpr let_value_t let_value{};
    inline constexpr let_error_t let_error{};
    inline constexpr let_stopped_t let_stopped{};
    using detail::on::on_t;
    inline constexpr on_t on{};
    using detail::schd_from::schedule_from_t;
    inline constexpr schedule_from_t schedule_from{};
    using detail::xfer::transfer_t;
    inline constexpr transfer_t transfer{};
    using detail::into_var::into_variant_t;
    inline constexpr into_variant_t into_variant{};
    using detail::stopped_as::stopped_as_optional_t;
    using detail::stopped_as::stopped_as_error_t;
    inline constexpr stopped_as_optional_t stopped_as_optional{};
    inline constexpr stopped_as_error_t stopped_as_error{};
    using detail::start_det::start_detached_t;
    inline constexpr start_detached_t start_detached{};
    using detail::execute::execute_t;
    inline constexpr execute_t execute{};
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

        namespace loop = exec::detail::loop;

        struct env_t
        {
            loop::schd_t schd;
            friend loop::schd_t tag_invoke(exec::get_scheduler_t, const env_t& self) noexcept { return self.schd; }
            friend loop::schd_t tag_invoke(exec::get_delegatee_scheduler_t, const env_t& self) noexcept
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
            type(loop::run_loop* loop, variant_t<S>* ptr): loop_(loop), ptr_(ptr) {}

        private:
            loop::run_loop* loop_;
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
                    loop::run_loop ctx;
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
