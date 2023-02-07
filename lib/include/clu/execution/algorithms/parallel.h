#pragma once

#include "basic.h"

namespace clu::exec
{
    namespace detail
    {
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

            template <typename R>
            using recv_env_t = env_t<env_of_t<R>>; // gcc bug workaround

            template <typename R, typename S, std::size_t I, typename... Ts>
            class recv_t_<R, S, I, Ts...>::type
            {
            public:
                explicit type(ops_t<R, Ts...>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<R, Ts...>* ops_;

                const R& get_base() const noexcept;
                recv_env_t<R> get_env() const noexcept;
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

                template <recvs::completion_cpo SetCpo, typename... Args>
                friend void tag_invoke(SetCpo, type&& self, Args&&... args) noexcept
                {
                    self.ops_->template set<I>(SetCpo{}, static_cast<Args&&>(args)...);
                }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.get_base(), static_cast<Args&&>(args)...);
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
                template <forwarding<R> R2, typename... Us>
                explicit type(R2&& recv, Us&&... snds):
                    recv_(static_cast<R2&&>(recv)),
                    children_(when_all::connect_children<R>(
                        std::index_sequence_for<Ts...>{}, this, static_cast<Us&&>(snds)...)) {}
                // clang-format on

                const R& get_recv() const noexcept { return recv_; }

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
                using callback_t = typename stop_token_of_t<env_of_t<R>>::template callback_type<stop_callback>;

                R recv_;
                in_place_stop_source stop_src_;
                children_ops_t<R, Ts...> children_;
                std::atomic_size_t finished_count_{};
                std::atomic<final_signal> signal_{};
                std::optional<callback_t> callback_;
                CLU_NO_UNIQUE_ADDRESS values_t values_;
                error_t error_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    self.callback_.emplace( // Propagate stop signal
                        exec::get_stop_token(exec::get_env(self.recv_)), //
                        stop_callback{self.stop_src_});
                    if (self.stop_src_.stop_requested()) // Shortcut when the operation is preemptively stopped
                    {
                        self.callback_.reset();
                        exec::set_stopped(static_cast<R&&>(self.recv_));
                        return;
                    }
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
                                    if constexpr (std::is_same_v<E, std::monostate>)
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
            const R& recv_t_<R, S, I, Ts...>::type::get_base() const noexcept
            {
                return ops_->get_recv();
            }

            template <typename R, typename S, std::size_t I, typename... Ts>
            recv_env_t<R> recv_t_<R, S, I, Ts...>::type::get_env() const noexcept
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
                            return exec::just(); // sends nothing
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

            template <typename R>
            using recv_env_t = env_t<env_of_t<R>>; // gcc 12.2 bug workaround

            template <typename R, typename S, std::size_t I, typename... Ts>
            class recv_t_<R, S, I, Ts...>::type
            {
            public:
                explicit type(ops_t<R, Ts...>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<R, Ts...>* ops_;

                const R& get_base() const noexcept;
                recv_env_t<R> get_env() const noexcept;
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

                template <recvs::completion_cpo SetCpo, typename... Args>
                friend void tag_invoke(SetCpo, type&& self, Args&&... args) noexcept
                {
                    self.ops_->template set<I>(SetCpo{}, static_cast<Args&&>(args)...);
                }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.get_base(), static_cast<Args&&>(args)...);
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

            template <typename R, typename... Ts>
            class ops_t_<R, Ts...>::type
            {
            public:
                // clang-format off
                template <forwarding<R> R2, typename... Us>
                explicit type(R2&& recv, Us&&... snds):
                    recv_(static_cast<R2&&>(recv)),
                    children_(when_any::connect_children<R>(
                        std::index_sequence_for<Ts...>{}, this, static_cast<Us&&>(snds)...)) {}
                // clang-format on

                const R& get_recv() const noexcept { return recv_; }

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
                using callback_t = typename stop_token_of_t<env_of_t<R>>::template callback_type<stop_callback>;

                R recv_;
                in_place_stop_source stop_src_;
                children_ops_t<R, Ts...> children_;
                std::atomic_size_t finished_count_{};
                std::atomic<final_signal> signal_{};
                std::optional<callback_t> callback_;
                CLU_NO_UNIQUE_ADDRESS values_t values_;
                error_t error_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    self.callback_.emplace( // Propagate stop signal
                        exec::get_stop_token(exec::get_env(self.recv_)), //
                        stop_callback{self.stop_src_});
                    if (self.stop_src_.stop_requested()) // Shortcut when the operation is preemptively stopped
                    {
                        self.callback_.reset();
                        exec::set_stopped(static_cast<R&&>(self.recv_));
                        return;
                    }
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
                                    if constexpr (std::is_same_v<E, std::monostate>)
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
            const R& recv_t_<R, S, I, Ts...>::type::get_base() const noexcept
            {
                return ops_->get_recv();
            }

            template <typename R, typename S, std::size_t I, typename... Ts>
            recv_env_t<R> recv_t_<R, S, I, Ts...>::type::get_env() const noexcept
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
                            return just_t{}(); // sends nothing
                        else
                            return snd_t<Ts...>(static_cast<Ts&&>(snds)...);
                    }
                }
            };
        } // namespace when_any

        namespace stop_when
        {
            template <typename Env>
            using env_t = adapted_env_t<Env, get_stop_token_t, in_place_stop_token>;

            template <typename S, typename T, typename Env>
            using comp_sigs = make_completion_signatures<S, env_t<Env>,
                make_completion_signatures<T, env_t<Env>,
                    completion_signatures<set_error_t(std::exception_ptr), set_stopped_t()>,
                    meta::constant_q<completion_signatures<>>::fn>>;

            template <typename Cpo, typename... Ts>
            decayed_tuple<Cpo, Ts...> storage_tuple_impl(Cpo (*)(Ts...));
            template <typename Sig>
            using storage_tuple = decltype(stop_when::storage_tuple_impl(static_cast<Sig*>(nullptr)));

            template <typename S, typename T, typename R>
            using storage_variant = meta::unpack_invoke< //
                meta::transform_l<comp_sigs<S, T, env_of_t<R>>, meta::quote1<storage_tuple>>,
                meta::quote<nullable_variant>>;

            template <typename S, typename T, typename R>
            struct recv_t_
            {
                class type;
            };

            template <typename S, typename T, typename R>
            using recv_t = typename recv_t_<S, T, std::decay_t<R>>::type;

            template <typename S, typename T, typename R>
            struct trig_recv_t_
            {
                class type;
            };

            template <typename S, typename T, typename R>
            using trig_recv_t = typename trig_recv_t_<S, T, std::decay_t<R>>::type;

            template <typename S, typename T, typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename T, typename R>
            using ops_t = typename ops_t_<S, T, R>::type;

            template <typename R>
            using recv_env_t = env_t<env_of_t<R>>; // gcc 12.2 bug workaround

            // TODO: recv_t and trig_recv_t should share some code

            template <typename S, typename T, typename R>
            class recv_t_<S, T, R>::type
            {
            public:
                explicit type(ops_t<S, T, R>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, T, R>* ops_;

                const R& get_base() const noexcept;

                recv_env_t<R> get_env() const noexcept;
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

                template <recvs::completion_cpo Cpo, typename... Ts>
                friend void tag_invoke(Cpo, type&& self, Ts&&... args) noexcept
                {
                    self.ops_->set(Cpo{}, static_cast<Ts&&>(args)...);
                }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.get_base(), static_cast<Args&&>(args)...);
                }
            };

            template <typename S, typename T, typename R>
            class trig_recv_t_<S, T, R>::type
            {
            public:
                explicit type(ops_t<S, T, R>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, T, R>* ops_;

                const R& get_base() const noexcept;

                recv_env_t<R> get_env() const noexcept;
                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

                template <recvs::completion_cpo Cpo, typename... Ts>
                friend void tag_invoke(Cpo, type&& self, Ts&&... args) noexcept
                {
                    if constexpr (std::is_same_v<Cpo, set_error_t>)
                        self.ops_->set(set_error, static_cast<Ts&&>(args)...);
                    else // value and stopped all map to stopped for the trigger
                        self.ops_->set(set_stopped);
                }

                template <recv_qry::fwd_recv_query Tag, typename... Args>
                    requires callable<Tag, const R&, Args...>
                constexpr friend decltype(auto) tag_invoke(Tag cpo, const type& self, Args&&... args) noexcept(
                    nothrow_callable<Tag, const R&, Args...>)
                {
                    return cpo(self.get_base(), static_cast<Args&&>(args)...);
                }
            };

            template <typename S, typename T, typename R>
            class ops_t_<S, T, R>::type
            {
            public:
                // clang-format off
                template <typename S2, typename T2, typename R2>
                type(S2&& src, T2&& trigger, R2&& recv):
                    recv_(static_cast<R2&&>(recv)),
                    src_ops_(connect(static_cast<S2&&>(src), recv_t<S, T, R>(this))),
                    trig_ops_(connect(static_cast<T2&&>(trigger), trig_recv_t<S, T, R>(this))) {}
                // clang-format on

                const R& get_recv() noexcept { return recv_; }
                env_t<env_of_t<R>> get_recv_env() noexcept { return {get_env(recv_), stop_src_.get_token()}; }

                template <typename Cpo, typename... Ts>
                void set(Cpo, Ts&&... args) noexcept
                {
                    while (true)
                        switch (counter_.fetch_add(1, std::memory_order::acq_rel))
                        {
                            case 0:
                            {
                                stop_src_.request_stop();
                                storage_.template emplace<decayed_tuple<Cpo, Ts...>>(Cpo{}, static_cast<Ts&&>(args)...);
                                continue;
                            }
                            case 2: set_recv(); return;
                            default /* case 1 */: return;
                        }
                }

            private:
                struct stop_callback
                {
                    in_place_stop_source& stop_src;
                    void operator()() const noexcept { stop_src.request_stop(); }
                };
                using callback_t = typename stop_token_of_t<env_of_t<R>>::template callback_type<stop_callback>;

                R recv_;
                std::atomic_uint8_t counter_{0};
                storage_variant<S, T, R> storage_;
                in_place_stop_source stop_src_;
                std::optional<callback_t> callback_;
                connect_result_t<S, recv_t<S, T, R>> src_ops_;
                connect_result_t<T, trig_recv_t<S, T, R>> trig_ops_;

                void set_recv() noexcept
                {
                    callback_.reset();
                    std::visit(
                        [&]<typename Tup>(Tup&& tuple) noexcept
                        {
                            if constexpr (std::is_same_v<Tup, std::monostate>)
                                unreachable();
                            else
                                std::apply([&]<typename Cpo, typename... Ts>(Cpo, Ts&&... args) noexcept
                                    { Cpo{}(static_cast<R&&>(recv_), static_cast<Ts&&>(args)...); },
                                    static_cast<Tup&&>(tuple));
                        },
                        std::move(storage_));
                }

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    self.callback_.emplace( // Propagate stop signal
                        exec::get_stop_token(exec::get_env(self.recv_)), //
                        stop_callback{self.stop_src_});
                    if (self.stop_src_.stop_requested()) // Shortcut when the operation is preemptively stopped
                    {
                        self.callback_.reset();
                        exec::set_stopped(static_cast<R&&>(self.recv_));
                        return;
                    }
                    start(self.src_ops_);
                    start(self.trig_ops_);
                }
            };

            template <typename S, typename T, typename R>
            const R& recv_t_<S, T, R>::type::get_base() const noexcept
            {
                return ops_->get_recv();
            }

            template <typename S, typename T, typename R>
            recv_env_t<R> recv_t_<S, T, R>::type::get_env() const noexcept
            {
                return ops_->get_recv_env();
            }

            template <typename S, typename T, typename R>
            const R& trig_recv_t_<S, T, R>::type::get_base() const noexcept
            {
                return ops_->get_recv();
            }

            template <typename S, typename T, typename R>
            recv_env_t<R> trig_recv_t_<S, T, R>::type::get_env() const noexcept
            {
                return ops_->get_recv_env();
            }

            template <typename S, typename T>
            struct snd_t_
            {
                class type;
            };

            template <typename S, typename T>
            using snd_t = typename snd_t_<std::decay_t<S>, std::decay_t<T>>::type;

            template <typename S, typename T>
            class snd_t_<S, T>::type
            {
            public:
                // clang-format off
                template <typename S2, typename T2>
                type(S2&& src, T2&& trigger):
                    src_(static_cast<S2&&>(src)), trigger_(static_cast<T2&&>(trigger)) {}
                // clang-format on

            private:
                S src_;
                T trigger_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return ops_t<S, T, R>(
                        static_cast<S&&>(self.src_), static_cast<T&&>(self.trigger_), static_cast<R&&>(recv));
                }

                // clang-format off
                template <typename Env>
                friend make_completion_signatures<S, env_t<Env>,
                    make_completion_signatures<T, env_t<Env>,
                        completion_signatures<set_error_t(std::exception_ptr), set_stopped_t()>,
                        meta::constant_q<completion_signatures<>>::fn>>
                tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept { return {}; }
                // clang-format on
            };

            struct stop_when_t
            {
                template <sender S, sender T>
                auto operator()(S&& src, T&& trigger) const
                {
                    return snd_t<S, T>(static_cast<S&&>(src), static_cast<T&&>(trigger));
                }

                template <sender T>
                auto operator()(T&& trigger) const
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<T&&>(trigger)));
                }
            };
        } // namespace stop_when

        namespace dtch_cncl
        {
            template <typename S, typename R, typename F>
            struct recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename F>
            using recv_t = typename recv_t_<S, std::decay_t<R>, F>::type;

            template <typename S, typename R, typename F>
            struct cleanup_recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename F>
            using cleanup_recv_t = typename cleanup_recv_t_<S, R, F>::type;

            template <typename S, typename R, typename F>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R, typename F>
            using ops_t = typename ops_t_<S, R, F>::type;

            template <typename S, typename R, typename F>
            struct shared_state;

            template <typename S, typename R, typename F>
            struct stop_callback
            {
                shared_state<S, R, F>& shst;
                void operator()() const noexcept;
            };

            template <typename S, typename R, typename F>
            struct shared_state
            {
                using callback_type =
                    typename stop_token_of_t<env_of_t<R>>::template callback_type<stop_callback<S, R, F>>;

                template <typename... Ts>
                using ops_type_of_values = connect_result_t<std::invoke_result_t<F, Ts...>, cleanup_recv_t<S, R, F>>;

                R recv;
                F func;
                std::atomic_flag value_sent;
                ops_optional<connect_result_t<S, recv_t<S, R, F>>> ops;
                value_types_of_t<S, env_of_t<R>, ops_type_of_values, nullable_ops_variant> cleanup_ops;
                std::optional<callback_type> cb;

                // clang-format off
                template <typename R2, typename F2>
                shared_state(R2&& recv, F2&& func):
                    recv(static_cast<R2&&>(recv)), func(static_cast<F2&&>(func)) {}
                // clang-format on
            };

            template <typename S, typename R, typename F>
            class ops_t_<S, R, F>::type
            {
            public:
                template <typename S2, typename R2, typename F2>
                type(S2&& snd, R2&& recv, F2&& func):
                    shst_(std::allocate_shared< //
                        shared_state<S, R, F>, call_result_t<get_allocator_t, env_of_t<R>>>(
                        get_allocator(get_env(recv)), //
                        static_cast<R2&&>(recv), static_cast<F2&&>(func)))
                {
                    shst_->ops.emplace_with( //
                        [&] { return connect(static_cast<S2&&>(snd), recv_t<S, R, F>(shst_)); });
                }

            private:
                std::shared_ptr<shared_state<S, R, F>> shst_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    // Cache the shared state in case we get destroyed by starting the operation
                    const auto shst = std::move(self.shst_);
                    shst->cb.emplace(get_stop_token(get_env(shst->recv)), stop_callback<S, R, F>{*shst});
                    start(*shst->ops);
                }
            };

            template <typename S, typename R, typename F>
            void stop_callback<S, R, F>::operator()() const noexcept
            {
                if (shst.value_sent.test_and_set(std::memory_order_release)) // We're not the first
                    return;
                // Send a stopped signal to the receiver and let the detached operation run till completion
                set_stopped(static_cast<R&&>(shst.recv));
            }

            template <typename S, typename R, typename F>
            class recv_t_<S, R, F>::type
            {
            public:
                explicit type(std::shared_ptr<shared_state<S, R, F>> shst) noexcept: shst_(std::move(shst)) {}

            private:
                std::shared_ptr<shared_state<S, R, F>> shst_;

                template <recvs::completion_cpo Cpo, typename... Ts>
                friend void tag_invoke(Cpo, type&& self, Ts&&... args) noexcept
                {
                    // We're detached, forward the results to the cleanup factory
                    if (self.shst_->value_sent.test_and_set(std::memory_order_release))
                    {
                        if constexpr (std::is_same_v<Cpo, set_value_t>)
                            self.set_value_detached(static_cast<Ts&&>(args)...);
                        else if constexpr (std::is_same_v<Cpo, set_error_t>)
                            std::terminate();
                        else // set_stopped
                            self.shst_.reset();
                    }
                    // All is good, forward the args to the upstream receiver
                    else
                    {
                        Cpo{}(static_cast<R&&>(self.shst_->recv), static_cast<Ts&&>(args)...);
                        self.shst_.reset(); // This effectively destroys *this
                    }
                }

                friend env_of_t<R> tag_invoke(get_env_t, const type& self) noexcept
                {
                    return get_env(self.shst_->recv);
                }

                template <typename... Ts>
                void set_value_detached(Ts&&... args) noexcept;
            };

            template <typename S, typename R, typename F>
            class cleanup_recv_t_<S, R, F>::type
            {
            public:
                // clang-format off
                explicit type(std::shared_ptr<shared_state<S, R, F>> shst) noexcept:
                    shst_(std::move(shst)),
                    env_(exec::make_env(get_env(shst_->recv), get_stop_token, never_stop_token{})) {}
                // clang-format on

            private:
                std::shared_ptr<shared_state<S, R, F>> shst_;
                CLU_NO_UNIQUE_ADDRESS adapted_env_t<env_of_t<R>, get_stop_token_t, never_stop_token> env_{};

                friend void tag_invoke(set_value_t, type&& self) noexcept { self.shst_.reset(); }
                friend void tag_invoke(set_error_t, type&&, auto&&) noexcept { std::terminate(); }
                friend void tag_invoke(set_stopped_t, type&& self) noexcept { self.shst_.reset(); }
                friend const auto& tag_invoke(get_env_t, const type& self) noexcept { return self.env_; }
            };

            template <typename S, typename R, typename F>
            template <typename... Ts>
            void recv_t_<S, R, F>::type::set_value_detached(Ts&&... args) noexcept
            {
                using shst_t = shared_state<S, R, F>;
                using cleanup_ops_t = typename shst_t::template ops_type_of_values<Ts...>;
                shst_t& shst = *shst_;
                start(shst.cleanup_ops.template emplace<cleanup_ops_t>(copy_elider{[&]
                    {
                        return connect( //
                            std::invoke(static_cast<F&&>(shst.func), static_cast<Ts&&>(args)...),
                            cleanup_recv_t<S, R, F>(std::move(shst_)));
                    }}));
            }

            template <typename S, typename F>
            struct snd_t_
            {
                class type;
            };

            template <typename S, typename F>
            using snd_t = typename snd_t_<std::decay_t<S>, std::decay_t<F>>::type;

            template <typename S, typename F>
            class snd_t_<S, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename F2>
                type(S2&& snd, F2&& func): snd_(static_cast<S2&&>(snd)), func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                S snd_;
                F func_;

                template <typename R> // TODO: constrain R s.t. F is a cleanup factory for (S, Env)
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return ops_t<S, R, F>( //
                        static_cast<S&&>(self.snd_), static_cast<R&&>(recv), static_cast<F&&>(self.func_));
                }

                // clang-format off
                template <typename Env>
                friend make_completion_signatures<S, Env,
                    completion_signatures<set_error_t(std::exception_ptr), set_stopped_t()>>
                tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept { return {}; }
                // clang-format on
            };

            inline constexpr auto noop_cleanup_factory = [](auto&&...) noexcept { return just_void; };
            using noop_cleanup_factory_t = decltype(noop_cleanup_factory);

            struct detach_on_stop_request_t
            {
                template <sender S, typename F>
                auto operator()(S&& snd, F&& cleanup_factory) const
                {
                    return snd_t<S, F>(static_cast<S&&>(snd), static_cast<F&&>(cleanup_factory));
                }

                template <sender S>
                auto operator()(S&& snd) const
                {
                    return (*this)(static_cast<S&&>(snd), noop_cleanup_factory);
                }

                template <typename F>
                auto operator()(F&& cleanup_factory) const
                {
                    return clu::make_piper(clu::bind_back(*this, cleanup_factory));
                }

                auto operator()() const noexcept { return (*this)(noop_cleanup_factory); }
            };
        } // namespace dtch_cncl

        namespace bulk
        {
            // TODO: implement
        } // namespace bulk

        namespace split
        {
            // TODO: implement
        } // namespace split
    } // namespace detail

    using detail::when_all::when_all_t;
    using detail::when_any::when_any_t;
    using detail::stop_when::stop_when_t;
    using detail::dtch_cncl::detach_on_stop_request_t;

    inline constexpr when_all_t when_all{};
    inline constexpr when_any_t when_any{};
    inline constexpr stop_when_t stop_when{};
    inline constexpr detach_on_stop_request_t detach_on_stop_request{};

    using detail::dtch_cncl::noop_cleanup_factory_t;
    using detail::dtch_cncl::noop_cleanup_factory;
} // namespace clu::exec
