#pragma once

#include "../utility.h"
#include "../../copy_elider.h"
#include "../../piper.h"

namespace clu::exec
{
    namespace detail
    {
#define CLU_EXEC_FWD_ENV(member)                                                                                       \
    decltype(auto) tag_invoke(get_env_t) const CLU_SINGLE_RETURN(clu::adapt_env(get_env(this->member)))

        namespace just
        {
            template <typename R, typename Cpo, typename... Ts>
            class ops_t
            {
            public:
                // clang-format off
                template <typename Tup>
                ops_t(R&& recv, Tup&& values) noexcept(
                    std::is_nothrow_move_constructible_v<R> &&
                    std::is_nothrow_constructible_v<std::tuple<Ts...>, Tup>):
                    recv_(static_cast<R&&>(recv)),
                    values_(static_cast<Tup&&>(values)) {}
                // clang-format on

                void tag_invoke(start_t) & noexcept
                {
                    std::apply([&](Ts&&... values) //
                        { Cpo{}(std::move(*this).recv_, static_cast<Ts&&>(values)...); },
                        std::move(*this).values_);
                }

            private:
                CLU_NO_UNIQUE_ADDRESS R recv_;
                CLU_NO_UNIQUE_ADDRESS std::tuple<Ts...> values_;
            };

            template <typename Cpo, typename... Ts>
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <forwarding<Ts>... Us>
                constexpr explicit snd_t_(Us&&... args) noexcept(
                    std::is_nothrow_constructible_v<std::tuple<Ts...>, Us...>):
                    values_(static_cast<Us&&>(args)...) {}

                template <typename R>
                auto tag_invoke(connect_t, R&& recv) const& CLU_SINGLE_RETURN(
                    ops_t<R, Cpo, Ts...>{static_cast<R&&>(recv), values_});

                template <typename R>
                auto tag_invoke(connect_t, R&& recv) && CLU_SINGLE_RETURN(
                    ops_t<R, Cpo, Ts...>{static_cast<R&&>(recv), std::move(values_)});

                completion_signatures<Cpo(Ts...)> tag_invoke(
                    get_completion_signatures_t, auto) const noexcept { return {}; }
                // clang-format on

                std::true_type tag_invoke(completes_inline_t) const noexcept { return {}; }

            private:
                CLU_NO_UNIQUE_ADDRESS std::tuple<Ts...> values_;
            };

            template <typename Cpo, typename... Ts>
            using snd_t = snd_t_<Cpo, std::decay_t<Ts>...>;

            struct just_t
            {
                template <typename... Ts>
                CLU_STATIC_CALL_OPERATOR(auto)
                (Ts&&... values)
                {
                    return snd_t<set_value_t, Ts...>(static_cast<Ts&&>(values)...);
                }
            };

            struct just_error_t
            {
                template <movable_value E>
                CLU_STATIC_CALL_OPERATOR(auto)
                (E&& error)
                {
                    return snd_t<set_error_t, E>(static_cast<E&&>(error));
                }
            };

            struct just_stopped_t
            {
                constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return snd_t<set_stopped_t>(); }
            };
        } // namespace just

        namespace stop_req
        {
            template <typename R>
            class ops_t_
            {
            public:
                template <typename R2>
                explicit ops_t_(R2&& recv): recv_(static_cast<R2&&>(recv))
                {
                }

                void tag_invoke(start_t) & noexcept
                {
                    if (get_stop_token(get_env(recv_)).stop_requested())
                        exec::set_stopped(static_cast<R&&>(recv_));
                    else
                        exec::set_value(static_cast<R&&>(recv_));
                }

            private:
                R recv_;
            };

            template <typename R>
            using ops_t = ops_t_<std::remove_cvref_t<R>>;

            struct snd_t
            {
                using is_sender = void;

                template <typename R>
                auto tag_invoke(connect_t, R&& recv) const noexcept(std::is_nothrow_move_constructible_v<R>)
                {
                    if constexpr (unstoppable_token<stop_token_of_t<env_of_t<R>>>)
                        return exec::connect(just::just_t{}(), static_cast<R&&>(recv));
                    else
                        return ops_t<R>(static_cast<R&&>(recv));
                }

                template <typename Env>
                auto tag_invoke(get_completion_signatures_t, Env&&) const noexcept
                {
                    if constexpr (unstoppable_token<stop_token_of_t<Env>>)
                        return completion_signatures<set_value_t()>{};
                    else
                        return completion_signatures<set_value_t(), set_stopped_t()>{};
                }
            };

            struct stop_if_requested_t
            {
                constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return snd_t{}; }
            };
        } // namespace stop_req

        namespace upon
        {
            template <typename T, bool NoThrow = false>
            using sigs_of_single = completion_signatures<
                conditional_t<std::is_void_v<T>, set_value_t(), set_value_t(with_regular_void_t<T>)>>;

            template <typename F, typename... Args>
            using sigs_of_inv =
                maybe_add_throw<sigs_of_single<std::invoke_result_t<F, Args...>>, nothrow_invocable<F, Args...>>;

            template <typename R, typename Cpo, typename F>
            class recv_t_
            {
            public:
                using is_receiver = void;

                // clang-format off
                template <typename R2, typename F2>
                recv_t_(R2&& recv, F2&& func) noexcept(
                    std::is_nothrow_constructible_v<R, R2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    recv_(static_cast<R2&&>(recv)),
                    func_(static_cast<F2&&>(func)) {}
                // clang-format on

                // Transformed
                template <typename... Args>
                    requires std::invocable<F, Args...> && receiver_of<R, sigs_of_inv<F, Args...>>
                void tag_invoke(Cpo, Args&&... args) && noexcept
                {
                    // clang-format off
                    if constexpr (nothrow_invocable<F, Args...>)
                        this->set_impl(static_cast<Args&&>(args)...);
                    else
                        try { this->set_impl(static_cast<Args&&>(args)...); }
                        catch (...) { exec::set_error(static_cast<R&&>(recv_), std::current_exception()); }
                    // clang-format on
                }

                CLU_EXEC_FWD_ENV(recv_); // Pass through

                template <completion_cpo Tag, typename... Args>
                    requires(!std::same_as<Tag, Cpo>) && receiver_of<R, completion_signatures<Tag(Args...)>>
                constexpr void tag_invoke(Tag cpo, Args&&... args) && noexcept
                {
                    cpo(static_cast<R&&>(recv_), static_cast<Args&&>(args)...);
                }

            private:
                CLU_NO_UNIQUE_ADDRESS R recv_;
                CLU_NO_UNIQUE_ADDRESS F func_;

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

            template <typename R, typename Cpo, typename F>
            using recv_t = recv_t_<std::remove_cvref_t<R>, Cpo, F>;

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
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <typename S2, typename F2>
                constexpr snd_t_(S2&& snd, F2&& func) noexcept(
                    std::is_nothrow_constructible_v<S, S2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    snd_(static_cast<S2&&>(snd)),
                    func_(static_cast<F2&&>(func)) {}
                // clang-format on

                template <typename R>
                    requires sender_to<const S&, recv_t<R, Cpo, F>>
                auto tag_invoke(connect_t, R&& recv) const& noexcept(nothrow_connectable<S, recv_t<R, Cpo, F>>)
                {
                    return exec::connect(snd_, recv_t<R, Cpo, F>(static_cast<R&&>(recv), func_));
                }

                template <typename R>
                    requires sender_to<S&&, recv_t<R, Cpo, F>>
                auto tag_invoke(connect_t, R&& recv) && noexcept(nothrow_connectable<S, recv_t<R, Cpo, F>>)
                {
                    return exec::connect(std::move(snd_), recv_t<R, Cpo, F>(static_cast<R&&>(recv), std::move(func_)));
                }

                CLU_EXEC_FWD_ENV(snd_);

                using make_sig = make_sig_impl<Cpo, F>;

                template <typename Env>
                constexpr make_completion_signatures<S, Env, completion_signatures<>, //
                    make_sig::template value_sig, make_sig::template error_sig, typename make_sig::stopped_sig>
                tag_invoke(get_completion_signatures_t, Env) const noexcept
                {
                    return {};
                }

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
                CLU_NO_UNIQUE_ADDRESS F func_;
            };

            template <typename S, typename Cpo, typename F>
            using snd_t = snd_t_<std::remove_cvref_t<S>, Cpo, std::decay_t<F>>;

            template <typename UponCpo, typename SetCpo>
            struct upon_t
            {
                template <sender S, typename F>
                constexpr CLU_STATIC_CALL_OPERATOR(auto)(S&& snd, F&& func)
                {
                    if constexpr (customized_sender_algorithm<UponCpo, SetCpo, S, F>)
                        return detail::invoke_customized_sender_algorithm<UponCpo, SetCpo>(
                            static_cast<S&&>(snd), static_cast<F&&>(func));
                    else
                        return snd_t<S, SetCpo, F>(static_cast<S&&>(snd), static_cast<F&&>(func));
                }

                template <typename F>
                constexpr CLU_STATIC_CALL_OPERATOR(auto)(F&& func)
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

        namespace mat
        {
            template <typename R>
            class recv_t_ : public receiver_adaptor<recv_t_<R>, R>
            {
            public:
                using receiver_adaptor<recv_t_, R>::receiver_adaptor;

                template <typename... Ts>
                void set_value(Ts&&... args) && noexcept
                {
                    if constexpr ((std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts> && ...))
                        exec::set_value(std::move(*this).base(), exec::set_value, static_cast<Ts&&>(args)...);
                    else
                        try
                        {
                            exec::set_value(std::move(*this).base(), exec::set_value, static_cast<Ts&&>(args)...);
                        }
                        catch (...)
                        {
                            exec::set_error(std::move(*this).base(), std::current_exception());
                        }
                }

                template <typename E>
                void set_error(E&& error) && noexcept
                {
                    exec::set_value(std::move(*this).base(), exec::set_error, static_cast<E&&>(error));
                }

                void set_stopped() && noexcept { exec::set_value(std::move(*this).base(), exec::set_stopped); }
            };

            template <typename R>
            using recv_t = recv_t_<std::decay_t<R>>;

            template <typename... Ts>
            using value_sig = filtered_sigs<set_value_t(set_value_t, Ts...),
                conditional_t<(std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts> && ...), //
                    void, set_error_t(std::exception_ptr)>>;

            template <typename E>
            using error_sig = completion_signatures<set_value_t(set_error_t, E)>;

            template <typename S>
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <forwarding<S> S2>
                explicit snd_t_(S2&& snd) noexcept(std::is_nothrow_constructible_v<S, S2>):
                    snd_(static_cast<S2&&>(snd)) {}

                template <typename R>
                auto tag_invoke(connect_t, R&& recv) &&
                    CLU_SINGLE_RETURN(connect(static_cast<S&&>(snd_), recv_t<R>(static_cast<R&&>(recv))));

                CLU_EXEC_FWD_ENV(snd_);

                template <typename Env>
                make_completion_signatures<S, Env, completion_signatures<>, //
                    value_sig, error_sig, completion_signatures<set_value_t(set_stopped_t)>>
                tag_invoke(get_completion_signatures_t, Env&&) const noexcept { return {}; }
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
            };

            template <typename S>
            using snd_t = snd_t_<std::decay_t<S>>;

            struct materialize_t
            {
                template <sender S>
                CLU_STATIC_CALL_OPERATOR(auto)
                (S&& snd) noexcept( //
                    std::is_nothrow_constructible_v<std::decay_t<S>, S>)
                {
                    return snd_t<S>(static_cast<S&&>(snd));
                }
                constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(materialize_t{}); }
            };
        } // namespace mat

        namespace demat
        {
            template <typename R>
            class recv_t_ : public receiver_adaptor<recv_t_<R>, R>
            {
            public:
                using receiver_adaptor<recv_t_, R>::receiver_adaptor;

                template <completion_cpo Cpo, typename... Ts>
                void set_value(Cpo, Ts&&... args) && noexcept
                {
                    if constexpr ((std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts> && ...))
                        Cpo{}(std::move(*this).base(), static_cast<Ts&&>(args)...);
                    else
                        try
                        {
                            Cpo{}(std::move(*this).base(), static_cast<Ts&&>(args)...);
                        }
                        catch (...)
                        {
                            exec::set_error(std::move(*this).base(), std::current_exception());
                        }
                }
            };

            template <typename R>
            using recv_t = recv_t_<std::decay_t<R>>;

            template <typename Cpo, typename... Ts>
            constexpr auto value_sig_impl() noexcept
            {
                if constexpr ((std::is_nothrow_constructible_v<std::decay_t<Ts>, Ts> && ...))
                    return completion_signatures<Cpo(Ts...)>{};
                else
                    return completion_signatures<Cpo(Ts...), set_error_t(std::exception_ptr)>{};
            }

            template <typename... Ts>
            using value_sig = decltype(value_sig_impl<Ts...>());

            template <typename S>
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <forwarding<S> S2>
                explicit snd_t_(S2&& snd) noexcept(std::is_nothrow_constructible_v<S, S2>):
                    snd_(static_cast<S2&&>(snd)) {}

                template <typename R>
                auto tag_invoke(connect_t, R&& recv) &&
                    CLU_SINGLE_RETURN(connect(static_cast<S&&>(snd_), recv_t<R>(static_cast<R&&>(recv))));

                CLU_EXEC_FWD_ENV(snd_);

                template <typename Env>
                make_completion_signatures<S, Env, completion_signatures<>, value_sig>
                tag_invoke(get_completion_signatures_t, Env&&) const noexcept { return {}; }
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
            };

            template <typename S>
            using snd_t = snd_t_<std::decay_t<S>>;

            struct dematerialize_t
            {
                template <sender S>
                CLU_STATIC_CALL_OPERATOR(auto)
                (S&& snd) noexcept( //
                    std::is_nothrow_constructible_v<std::decay_t<S>, S>)
                {
                    return snd_t<S>(static_cast<S&&>(snd));
                }
                constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(dematerialize_t{}); }
            };
        } // namespace demat

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
                template <typename E>
                using connect_result_for_err_t = connect_result_t<std::invoke_result_t<F, decay_lref<E>>, R>;
                template <typename... Es>
                using ops_variant_for = nullable_ops_variant<connect_result_for_err_t<Es>...>;

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
            class ops_t_;

            template <typename S, typename R, typename Cpo, typename F>
            using ops_t = ops_t_<S, std::remove_cvref_t<R>, Cpo, F>;

            template <typename F, typename Env, typename... Args>
            using recv_sigs = completion_signatures_of_t<std::invoke_result_t<F, decay_lref<Args>...>, Env>;

            template <typename S, typename R, typename Cpo, typename F>
            class recv_t_
            {
            public:
                using is_receiver = void;

                explicit recv_t_(ops_t<S, R, Cpo, F>* ops) noexcept: ops_(ops) {}

                // Transformed
                template <typename... Args>
                    requires std::invocable<F, Args...> && receiver_of<R, recv_sigs<F, env_of_t<R>, Args...>>
                void tag_invoke(Cpo, Args&&... args) && noexcept
                {
                    // clang-format off
                    if constexpr (receiver_of<R, completion_signatures<set_error_t(std::exception_ptr)>>)
                        try { this->set_impl(static_cast<Args&&>(args)...); }
                        catch (...) { exec::set_error(recv(), std::current_exception()); }
                    // clang-format on
                    else
                        this->set_impl(static_cast<Args&&>(args)...); // Shouldn't throw theoretically
                }

                CLU_EXEC_FWD_ENV(recv()); // Pass through

                // clang-format off
                template <completion_cpo Tag, typename... Args> requires
                    (!std::same_as<Tag, Cpo>) &&
                    receiver_of<R, completion_signatures<Tag(Args...)>>
                constexpr void tag_invoke(Tag cpo, Args&&... args) && noexcept
                {
                    cpo(recv(), static_cast<Args&&>(args)...);
                }
                // clang-format on

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
            };

            template <typename S, typename R, typename Cpo, typename F>
            using recv_t = recv_t_<S, std::remove_cvref_t<R>, Cpo, F>;

            template <typename S, typename R, typename Cpo, typename F>
            class ops_t_
            {
            public:
                // clang-format off
                template <typename R2, typename F2>
                ops_t_(S&& snd, R2&& recv, F2&& func) noexcept(
                    nothrow_connectable<S, recv_t<S, R, Cpo, F>> &&
                    std::is_nothrow_constructible_v<R, R2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    recv_(static_cast<R2&&>(recv)), func_(static_cast<F2&&>(func)),
                    initial_ops_(exec::connect(static_cast<S&&>(snd), recv_t<S, R, Cpo, F>(this))) {}
                // clang-format on

                void tag_invoke(start_t) & noexcept { exec::start(initial_ops_); }

            private:
                friend recv_t<S, R, Cpo, F>;

                R recv_;
                CLU_NO_UNIQUE_ADDRESS F func_;
                connect_result_t<S, recv_t<S, R, Cpo, F>> initial_ops_;
                storage<S, R, Cpo, F> storage_;
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
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <typename S2, typename F2>
                snd_t_(S2&& snd, F2&& func) noexcept(
                    std::is_nothrow_constructible_v<S, S2> &&
                    std::is_nothrow_constructible_v<F, F2>):
                    snd_(static_cast<S2&&>(snd)), func_(static_cast<F2&&>(func)) {}
                // clang-format on

                template <typename R>
                    requires sender_to<const S&, recv_t<S, R, Cpo, F>>
                auto tag_invoke(connect_t, R&& recv) const& CLU_SINGLE_RETURN(
                    ops_t<const S&, R, Cpo, F>(snd_, static_cast<R&&>(recv), func_));

                template <typename R>
                    requires sender_to<S&&, recv_t<S, R, Cpo, F>>
                auto tag_invoke(connect_t, R&& recv) &&
                    CLU_SINGLE_RETURN(ops_t<S, R, Cpo, F>(std::move(snd_), static_cast<R&&>(recv), std::move(func_)));

                CLU_EXEC_FWD_ENV(snd_);

                // clang-format off
                template <typename Env>
                constexpr make_completion_signatures<S, Env,
                    completion_signatures<set_error_t(std::exception_ptr)>,
                    make_sig_impl<Cpo, Env, F>::template value_sig,
                    make_sig_impl<Cpo, Env, F>::template error_sig,
                    typename make_sig_impl<Cpo, Env, F>::stopped_sig>
                tag_invoke(get_completion_signatures_t, Env) const noexcept { return {}; }
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
                CLU_NO_UNIQUE_ADDRESS F func_;
            };

            template <typename S, typename Cpo, typename F>
            using snd_t = snd_t_<std::remove_cvref_t<S>, Cpo, std::decay_t<F>>;

            template <typename LetCpo, typename SetCpo>
            struct let_t
            {
                // clang-format off
                template <sender S, typename F> requires
                    (!std::same_as<SetCpo, set_stopped_t>) ||
                    std::invocable<F>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& snd, F&& func)
                // clang-format on
                {
                    if constexpr (customized_sender_algorithm<LetCpo, SetCpo, S, F>)
                        return detail::invoke_customized_sender_algorithm<LetCpo, SetCpo>(
                            static_cast<S&&>(snd), static_cast<F&&>(func));
                    else
                        return snd_t<S, SetCpo, F>(static_cast<S&&>(snd), static_cast<F&&>(func));
                }

                template <typename F>
                constexpr CLU_STATIC_CALL_OPERATOR(auto)(F&& func)
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

        namespace qry_val
        {
            template <typename R, typename Q, typename T>
            class recv_t_ : public receiver_adaptor<recv_t_<R, Q, T>, R>
            {
            public:
                using is_receiver = void;

                // clang-format off
                template <typename R2, typename T2>
                recv_t_(R2&& recv, T2&& value):
                    receiver_adaptor<recv_t_, R>(static_cast<R2&&>(recv)),
                    env_(clu::get_env(receiver_adaptor<recv_t_, R>::base()), static_cast<T2&&>(value)) {}
                // clang-format on

                const auto& get_env() const noexcept { return env_; }

            private:
                adapted_env_t<env_of_t<R>, query_value<Q, T>> env_;
            };

            template <typename R, typename Q, typename T>
            using recv_t = recv_t_<std::decay_t<R>, Q, T>;

            template <typename S, typename Q, typename T>
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <typename S2, typename T2>
                snd_t_(S2&& snd, T2&& value):
                    snd_(static_cast<S2&&>(snd)), value_(static_cast<T2&&>(value)) {}
                // clang-format on

                template <typename R>
                auto tag_invoke(connect_t, R&& recv) &&
                {
                    return connect(static_cast<S&&>(snd_), //
                        recv_t<R, Q, T>(static_cast<R&&>(recv), static_cast<T&&>(value_)));
                }

                CLU_EXEC_FWD_ENV(snd_);

                // clang-format off
                template <typename Env>
                constexpr completion_signatures_of_t<S, adapted_env_t<Env, query_value<Q, T>>>
                tag_invoke(get_completion_signatures_t, Env&&) const noexcept { return {}; }
                // clang-format on

            private:
                S snd_;
                T value_;
            };

            template <typename S, typename Q, typename T>
            using snd_t = snd_t_<std::decay_t<S>, Q, std::decay_t<T>>;

            struct with_query_value_t
            {
                template <sender S, typename Q, typename T>
                CLU_STATIC_CALL_OPERATOR(auto)
                (S&& snd, Q, T&& value)
                {
                    return snd_t<S, Q, T>(static_cast<S&&>(snd), static_cast<T&&>(value));
                }

                template <typename Q, typename T>
                CLU_STATIC_CALL_OPERATOR(auto)
                (Q, T&& value)
                {
                    return clu::make_piper(clu::bind_back(with_query_value_t{}, //
                        Q{}, static_cast<T&&>(value)));
                }
            };
        } // namespace qry_val

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
            struct recv_t_ : receiver_adaptor<recv_t_<R, Var>, R>
            {
                using receiver_adaptor<recv_t_, R>::receiver_adaptor;

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

            template <typename R, typename Var>
            using recv_t = recv_t_<std::remove_cvref_t<R>, Var>;

            template <typename S>
            class snd_t_
            {
            public:
                using is_sender = void;

                // clang-format off
                template <typename S2>
                constexpr explicit snd_t_(S2&& snd) noexcept(std::is_nothrow_constructible_v<S, S2>):
                    snd_(static_cast<S2&&>(snd)) {}
                // clang-format on

                template <typename R>
                constexpr auto tag_invoke(connect_t, R&& recv) &&
                {
                    return exec::connect(
                        std::move(snd_), recv_t<R, variant_of<S, env_of_t<R>>>(static_cast<R&&>(recv)));
                }

                CLU_EXEC_FWD_ENV(snd_);

                template <typename Env>
                constexpr snd_comp_sigs<S, Env> tag_invoke(get_completion_signatures_t, Env) const
                {
                    return {};
                }

            private:
                CLU_NO_UNIQUE_ADDRESS S snd_;
            };

            template <typename S>
            using snd_t = snd_t_<std::remove_cvref_t<S>>;

            struct into_variant_t
            {
                template <sender S>
                CLU_STATIC_CALL_OPERATOR(auto)
                (S&& snd)
                {
                    return snd_t<S>(static_cast<S&&>(snd));
                }
                constexpr CLU_STATIC_CALL_OPERATOR(auto)() noexcept { return make_piper(into_variant_t{}); }
            };
        } // namespace into_var

#undef CLU_EXEC_FWD_ENV
    } // namespace detail

    using detail::just::just_t;
    using detail::just::just_error_t;
    using detail::just::just_stopped_t;
    using detail::stop_req::stop_if_requested_t;
    using detail::upon::then_t;
    using detail::upon::upon_error_t;
    using detail::upon::upon_stopped_t;
    using detail::mat::materialize_t;
    using detail::demat::dematerialize_t;
    using detail::let::let_value_t;
    using detail::let::let_error_t;
    using detail::let::let_stopped_t;
    using detail::qry_val::with_query_value_t;
    using detail::into_var::into_variant_t;

    inline constexpr just_t just{};
    inline constexpr just_error_t just_error{};
    inline constexpr just_stopped_t just_stopped{};
    inline constexpr stop_if_requested_t stop_if_requested{};
    inline constexpr just_stopped_t stop{};
    inline constexpr then_t then{};
    inline constexpr upon_error_t upon_error{};
    inline constexpr upon_stopped_t upon_stopped{};
    inline constexpr materialize_t materialize{};
    inline constexpr dematerialize_t dematerialize{};
    inline constexpr let_value_t let_value{};
    inline constexpr let_error_t let_error{};
    inline constexpr let_stopped_t let_stopped{};
    inline constexpr with_query_value_t with_query_value{};
    inline constexpr into_variant_t into_variant{};
} // namespace clu::exec
