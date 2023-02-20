#pragma once

#include "../utility.h"
#include "../../copy_elider.h"

namespace clu::exec
{
    namespace detail
    {
        namespace on
        {
#define CLU_EXEC_FWD_ENV(member)                                                                                       \
    friend decltype(auto) tag_invoke(get_env_t, const type& self)                                                      \
        CLU_SINGLE_RETURN(clu::adapt_env(get_env(self.member)))

            template <typename Env, typename Schd>
            using env_t = adapted_env_t<Env, query_value<get_scheduler_t, Schd>>;

            template <typename Schd, typename Env>
            auto replace_schd(Env&& env, Schd schd) noexcept
            {
                return clu::adapt_env(static_cast<Env&&>(env), //
                    query_value{get_scheduler, static_cast<Schd&&>(schd)});
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
                using is_receiver = void;

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
            using recv2_env_t = env_t<env_of_t<R>, Schd>;

            template <typename S, typename R, typename Schd>
            class recv2_t_<S, R, Schd>::type : public receiver_adaptor<type>
            {
            public:
                using is_receiver = void;

                explicit type(ops_t<S, R, Schd>* ops) noexcept: ops_(ops) {}

                const R& base() const& noexcept;
                R&& base() && noexcept;
                recv2_env_t<S, R, Schd> get_env() const noexcept;

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
            recv2_env_t<S, R, Schd> recv2_t_<S, R, Schd>::type::get_env() const noexcept
            {
                return on::replace_schd<Schd>(clu::get_env(base()), ops_->schd_);
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
                using is_sender = void;

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

                CLU_EXEC_FWD_ENV(snd_);

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
                CLU_STATIC_CALL_OPERATOR(auto)
                (Schd&& schd, Snd&& snd)
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
                meta::transform_l<add_sig<completion_signatures_of_t<S, env_of_t<R>>, set_error_t(std::exception_ptr)>,
                    meta::quote<storage_tuple_t>>,
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
                using is_receiver = void;

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

                CLU_EXEC_FWD_ENV(recv());
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
                    recv_(static_cast<R2&&>(recv), this),
                    schd_(static_cast<Schd&&>(schd)),
                    initial_ops_(exec::connect(static_cast<S&&>(snd), recv_t<S, R, Schd>(this))) {}
                // clang-format on

            private:
                friend recv_t<S, R, Schd>;
                friend recv2_t<S, R, Schd>;

                using final_ops_t = connect_result_t<schedule_result_t<Schd>, recv2_t<S, R, Schd>>;

                recv2_t<S, R, Schd> recv_;
                CLU_NO_UNIQUE_ADDRESS Schd schd_;
                connect_result_t<S, recv_t<S, R, Schd>> initial_ops_;
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
                using is_sender = void;

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

                // Customize completion scheduler
                friend auto tag_invoke(get_env_t, const type& self)
                    CLU_SINGLE_RETURN(clu::adapt_env(get_env(self.snd_), //
                        query_value{get_completion_scheduler<set_value_t>, self.schd_}, //
                        query_value{get_completion_scheduler<set_stopped_t>, self.schd_}));
            };

            struct schedule_from_t
            {
                template <scheduler Schd, sender Snd>
                CLU_STATIC_CALL_OPERATOR(auto)
                (Schd&& schd, Snd&& snd)
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

#undef CLU_EXEC_FWD_ENV
    } // namespace detail

    using detail::on::on_t;
    using detail::schd_from::schedule_from_t;

    inline constexpr on_t on{};
    inline constexpr schedule_from_t schedule_from{};
} // namespace clu::exec
