#pragma once

#include "basic.h"

namespace clu::exec
{
    namespace detail
    {
        template <typename Ops>
        struct cleanup_recv_t_
        {
            class type;
        };

        template <typename Ops>
        using cleanup_recv_t = typename cleanup_recv_t_<Ops>::type;

        template <typename Ops>
        class cleanup_recv_t_<Ops>::type
        {
        public:
            explicit type(Ops* ops) noexcept: ops_(ops) {}

        private:
            Ops* ops_;

            friend void tag_invoke(set_value_t, type&& self) noexcept { self.ops_->cleanup_done(); }
            friend void tag_invoke(set_error_t, type, auto&&) noexcept { std::terminate(); }
            friend void tag_invoke(set_stopped_t, type) noexcept { std::terminate(); }
            friend empty_env tag_invoke(get_env_t, type) noexcept { return {}; }
        };

        namespace adpt_nxt
        {
            template <typename S, typename F>
            struct stream_t_
            {
                class type;
            };

            template <typename S, typename F>
            using stream_t = typename stream_t_<std::decay_t<S>, std::decay_t<F>>::type;

            template <typename S, typename F>
            class stream_t_<S, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename F2>
                type(S2&& stream, F2&& func):
                    stream_(static_cast<S2&&>(stream)), func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S stream_;
                CLU_NO_UNIQUE_ADDRESS F func_;

                friend auto tag_invoke(next_t, type& self) { return self.func_(next(self.stream_)); }
                friend auto tag_invoke(cleanup_t, type& self) noexcept { return cleanup(self.stream_); }
            };

            struct adapt_next_t
            {
                template <stream S, typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, F&& func) 
                {
                    return stream_t<S, F>(static_cast<S&&>(strm), static_cast<F&&>(func));
                }

                template <typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(F&& func) 
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
                }
            };
        } // namespace adpt_nxt

        namespace adpt_cln
        {
            template <typename S, typename F>
            struct stream_t_
            {
                class type;
            };

            template <typename S, typename F>
            using stream_t = typename stream_t_<std::decay_t<S>, std::decay_t<F>>::type;

            template <typename S, typename F>
            class stream_t_<S, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename F2>
                type(S2&& stream, F2&& func):
                    stream_(static_cast<S2&&>(stream)), func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                S stream_;
                F func_;

                friend auto tag_invoke(next_t, type& self) { return next(self.stream_); }
                friend auto tag_invoke(cleanup_t, type& self) noexcept { return self.func_(cleanup(self.stream_)); }
            };

            struct adapt_cleanup_t
            {
                template <stream S, typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, F&& func) 
                {
                    return stream_t<S, F>(static_cast<S&&>(strm), static_cast<F&&>(func));
                }

                template <typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(F&& func) 
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
                }
            };
        } // namespace adpt_cln

        namespace rdc
        {
            template <typename S, typename R, typename T, typename F>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R, typename T, typename F>
            using ops_t = typename ops_t_<S, R, T, F>::type;

            template <typename S, typename R, typename T, typename F>
            struct next_recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename T, typename F>
            using next_recv_t = typename next_recv_t_<S, R, T, F>::type;

            template <typename S, typename R, typename T, typename F>
            class next_recv_t_<S, R, T, F>::type
            {
            public:
                explicit type(ops_t<S, R, T, F>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, R, T, F>* ops_;

                env_of_t<R> get_env() const noexcept; // Inherit stop token from parent receiver

                template <recvs::completion_cpo Cpo, typename... Ts>
                friend void tag_invoke(Cpo, type&& self, Ts&&... args) noexcept
                {
                    self.ops_->next_done(Cpo{}, static_cast<Ts&&>(args)...);
                }

                friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }
            };

            template <typename S, typename R, typename T, typename F>
            class ops_t_<S, R, T, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename R2, typename T2, typename F2>
                type(S2&& stream, R2&& recv, T2&& initial, F2&& func):
                    stream_(static_cast<S2&&>(stream)), recv_(static_cast<R2&&>(recv)),
                    current_(std::in_place_index<0>, static_cast<T2&&>(initial)),
                    func_(static_cast<F2&&>(func)), stoken_(get_stop_token(exec::get_env(recv_))) {}
                // clang-format on

                env_of_t<R> get_env() const noexcept { return exec::get_env(recv_); }

                template <forwarding<T> U>
                void next_done(set_value_t, U&& result) noexcept
                {
                    CLU_ASSERT(current_.index() == 0, //
                        "Corrupted state in stream reduce routine: "
                        "reduce(stream, initial, func). "
                        "This is probably a bug in clu :(");
                    if (stoken_.stop_requested()) // Early return
                    {
                        start_cleanup();
                        return;
                    }
                    try
                    {
                        std::get<0>(current_).value = static_cast<U&&>(result);
                        start_next();
                    }
                    catch (...) // The emplacement of the result failed, or the function call failed
                    {
                        current_.template emplace<std::exception_ptr>(std::current_exception());
                        start_cleanup();
                    }
                }

                template <typename E>
                void next_done(set_error_t, E&& error) noexcept
                {
                    current_.template emplace<std::decay_t<E>>(static_cast<E&&>(error));
                    start_cleanup();
                }

                void next_done(set_stopped_t) noexcept { start_cleanup(); }

                void cleanup_done() noexcept
                {
                    std::visit(
                        [&]<typename U>(U&& res) noexcept
                        {
                            if constexpr (std::is_same_v<U, value_wrapper>) // Storing a value
                            {
                                if (stoken_.stop_requested()) // Stop was requested
                                    set_stopped(static_cast<R&&>(recv_));
                                else
                                    set_value(static_cast<R&&>(recv_), static_cast<T&&>(res.value));
                            }
                            else // Storing an error
                                set_error(static_cast<R&&>(recv_), static_cast<U&&>(res));
                        },
                        std::move(current_));
                }

            private:
                struct value_wrapper
                {
                    CLU_NO_UNIQUE_ADDRESS T value;

                    // clang-format off
                    template <forwarding<T> T2>
                    explicit value_wrapper(T2&& val): value(static_cast<T2&&>(val)) {}
                    // clang-format on
                };

                struct let_value_fn
                {
                    F& func;
                    T& current;

                    template <typename... Args>
                    CLU_STATIC_CALL_OPERATOR(decltype(auto))(Args&&... args) 
                    {
                        return func(current, static_cast<Args&&>(args)...);
                    }
                };

                using stop_token_t = stop_token_of_t<env_of_t<R>>;
                using next_sender_t = call_result_t<let_value_t, next_result_t<S>, let_value_fn>;
                using current_t = meta::unpack_invoke< //
                    meta::unique_l<meta::concatenate_l< //
                        type_list<value_wrapper, std::exception_ptr>,
                        error_types_of_t<next_sender_t, env_of_t<R>, type_list>>>,
                    meta::quote<std::variant>>;
                using next_ops_t = connect_result_t<next_sender_t, next_recv_t<S, R, T, F>>;
                using cleanup_ops_t = connect_result_t<cleanup_result_t<S>, cleanup_recv_t<type>>;

                CLU_NO_UNIQUE_ADDRESS S stream_;
                CLU_NO_UNIQUE_ADDRESS R recv_;
                current_t current_;
                CLU_NO_UNIQUE_ADDRESS F func_;
                stop_token_t stoken_;
                nullable_ops_variant<next_ops_t, cleanup_ops_t> ops_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    if (self.stoken_.stop_requested())
                        self.start_cleanup(); // stop here
                    else
                        self.start_next();
                }

                void start_next() noexcept
                {
                    try
                    {
                        start(ops_.template emplace<next_ops_t>(copy_elider{[&]
                            {
                                return connect( //
                                    next(stream_) | let_value(let_value_fn{func_, std::get<0>(current_).value}),
                                    next_recv_t<S, R, T, F>(this));
                            }}));
                    }
                    catch (...) // The emplacement of next operation state failed
                    {
                        current_.template emplace<std::exception_ptr>(std::current_exception());
                        start_cleanup();
                    }
                }

                void start_cleanup() noexcept
                {
                    // If this emplacement fails std::terminate() would be automatically called
                    // since start_cleanup() is marked as noexcept.
                    start(ops_.template emplace<cleanup_ops_t>(
                        copy_elider{[&] { return connect(cleanup(stream_), cleanup_recv_t<type>(this)); }}));
                }
            };

            template <typename S, typename R, typename T, typename F>
            env_of_t<R> next_recv_t_<S, R, T, F>::type::get_env() const noexcept
            {
                return ops_->get_env();
            }

            template <typename S, typename T, typename F>
            struct snd_t_
            {
                class type;
            };

            template <typename S, typename T, typename F>
            using snd_t = typename snd_t_<std::decay_t<S>, std::decay_t<T>, std::decay_t<F>>::type;

            template <typename S, typename T, typename F>
            class snd_t_<S, T, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename T2, typename F2>
                type(S2&& stream, T2&& initial, F2&& func):
                    stream_(static_cast<S2&&>(stream)),
                    initial_(static_cast<T2&&>(initial)),
                    func_(static_cast<F2&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S stream_;
                CLU_NO_UNIQUE_ADDRESS T initial_;
                CLU_NO_UNIQUE_ADDRESS F func_;

                // clang-format off
                template <typename Env>
                friend make_completion_signatures<next_result_t<S>, Env,
                    completion_signatures<set_value_t(T), set_error_t(std::exception_ptr)>,
                    meta::constant_q<completion_signatures<>>::fn>
                tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept { return {}; }
                // clang-format on

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return ops_t<S, R, T, F>( //
                        static_cast<S&&>(self.stream_), static_cast<R&&>(recv), //
                        static_cast<T&&>(self.initial_), static_cast<F&&>(self.func_));
                }
            };

            struct reduce_t
            {
                template <stream S, typename T, typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, T&& initial, F&& func) 
                {
                    return snd_t<S, T, F>( //
                        static_cast<S&&>(strm), static_cast<T&&>(initial), static_cast<F&&>(func));
                }

                template <typename T, typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(T&& initial, F&& func) 
                {
                    return clu::make_piper(clu::bind_back(*this, //
                        static_cast<T&&>(initial), static_cast<F&&>(func)));
                }
            };
        } // namespace rdc

        namespace fltr
        {
            template <typename S, typename F>
            struct stream_t_
            {
                class type;
            };

            template <typename S, typename F>
            using stream_t = typename stream_t_<std::decay_t<S>, std::decay_t<F>>::type;

            template <typename S, typename F, typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename F, typename R>
            using ops_t = typename ops_t_<S, F, std::decay_t<R>>::type;

            template <typename S, typename F, typename R>
            struct next_recv_t_
            {
                class type;
            };

            template <typename S, typename F, typename R>
            using next_recv_t = typename next_recv_t_<S, F, R>::type;

            template <typename S, typename F, typename R>
            class next_recv_t_<S, F, R>::type : public receiver_adaptor<type>
            {
            public:
                explicit type(ops_t<S, F, R>* ops) noexcept: ops_(ops) {}

                // Propagate get_env, set_error, set_stopped to the base receiver
                const R& base() const& noexcept;
                R&& base() && noexcept;

                // Customize set_value
                template <typename... Ts>
                void set_value(Ts&&... args) noexcept;

            private:
                ops_t<S, F, R>* ops_;
            };

            template <typename S, typename F, typename R, typename Snd>
            struct pred_recv_t_
            {
                class type;
            };

            template <typename S, typename F, typename R, typename Snd>
            using pred_recv_t = typename pred_recv_t_<S, F, R, Snd>::type;

            template <typename S, typename F, typename R, typename Snd>
            class pred_recv_t_<S, F, R, Snd>::type : public receiver_adaptor<type>
            {
            public:
                explicit type(ops_t<S, F, R>* ops) noexcept: ops_(ops) {}

                // Propagate get_env, set_error, set_stopped to the base receiver
                const R& base() const& noexcept;
                R&& base() && noexcept;

                // Customize set_value
                void set_value(bool value) const noexcept;

            private:
                ops_t<S, F, R>* ops_;
            };

            template <typename S, typename F, typename R>
            class ops_t_<S, F, R>::type
            {
            public:
                // clang-format off
                template <typename R2>
                type(stream_t<S, F>* strm, R2&& recv) noexcept:
                    stream_(strm), recv_(static_cast<R2&&>(recv)) {}
                // clang-format on

                const R& recv() const& noexcept { return recv_; }
                R&& recv() && noexcept { return static_cast<R&&>(recv_); }

            private:
                friend next_recv_t<S, F, R>;

                template <typename... Ts>
                using pred_snd = std::invoke_result_t<F, const std::decay_t<Ts>&...>;
                template <typename... Ts>
                using pred_ops = connect_result_t<pred_snd<Ts...>, pred_recv_t<S, F, R, pred_snd<Ts...>>>;

                using next_ops = connect_result_t<next_result_t<S>, next_recv_t<S, F, R>>;

                stream_t<S, F>* stream_;
                CLU_NO_UNIQUE_ADDRESS R recv_;
                meta::unpack_invoke< //
                    meta::push_back_l< //
                        value_types_of_t<next_result_t<S>, env_of_t<R>, pred_ops, type_list>, next_ops>,
                    meta::quote<nullable_ops_variant>>
                    current_ops_;
                value_types_of_t<next_result_t<S>, env_of_t<R>, decayed_tuple, nullable_variant> values_;

                void start_next() noexcept
                {
                    try
                    {
                        start(current_ops_.template emplace<next_ops>(copy_elider{[&] { //
                            return connect(next(get_stream(*stream_)), next_recv_t<S, F, R>(this));
                        }}));
                    }
                    catch (...)
                    {
                        set_error(static_cast<R&&>(recv_), std::current_exception());
                    }
                }

                template <typename... Ts>
                void next_done(Ts&&... args) noexcept
                {
                    try
                    {
                        std::apply(
                            [&]<typename... Us>(Us&&... tuple_args)
                            {
                                using snd_t = pred_snd<Us...>;
                                using pred_ops_t = connect_result_t<snd_t, pred_recv_t<S, F, R, snd_t>>;
                                start(current_ops_.template emplace<pred_ops_t>(copy_elider{[&]
                                    {
                                        return connect( //
                                            std::invoke(get_func(*stream_), std::as_const(tuple_args)...),
                                            pred_recv_t<S, F, R, snd_t>(this));
                                    }}));
                            },
                            values_.template emplace<decayed_tuple<Ts...>>(static_cast<Ts&&>(args)...));
                    }
                    catch (...)
                    {
                        set_error(static_cast<R&&>(recv_), std::current_exception());
                    }
                }

                friend void predicate_done(type& self, const bool value) noexcept
                {
                    if (!value)
                        self.start_next();
                    else
                        std::visit(
                            [&]<typename Tup>(Tup&& tup)
                            {
                                if constexpr (std::is_same_v<Tup, std::monostate>)
                                    unreachable();
                                else
                                    std::apply([&]<typename... Ts>(Ts&&... values)
                                        { set_value(static_cast<R&&>(self.recv_), static_cast<Ts&&>(values)...); },
                                        static_cast<Tup&&>(tup));
                            },
                            std::move(self.values_));
                }

                friend void tag_invoke(start_t, type& self) noexcept { self.start_next(); }
            };

            // clang-format off
            template <typename S, typename F, typename R>
            const R& next_recv_t_<S, F, R>::type::base() const& noexcept { return ops_->recv_; }
            template <typename S, typename F, typename R>
            R&& next_recv_t_<S, F, R>::type::base() && noexcept { return static_cast<R&&>(ops_->recv_); }
            template <typename S, typename F, typename R, typename Snd>
            const R& pred_recv_t_<S, F, R, Snd>::type::base() const& noexcept { return ops_->recv(); }
            template <typename S, typename F, typename R, typename Snd>
            R&& pred_recv_t_<S, F, R, Snd>::type::base() && noexcept { return static_cast<R&&>(ops_)->recv(); }
            // clang-format on

            template <typename S, typename F, typename R>
            template <typename... Ts>
            void next_recv_t_<S, F, R>::type::set_value(Ts&&... args) noexcept
            {
                ops_->next_done(static_cast<Ts&&>(args)...);
            }

            template <typename S, typename F, typename R, typename Snd>
            void pred_recv_t_<S, F, R, Snd>::type::set_value(const bool value) const noexcept
            {
                predicate_done(*ops_, value);
            }

            template <typename S, typename F>
            struct snd_t_
            {
                class type;
            };

            template <typename S, typename F>
            using snd_t = typename snd_t_<S, F>::type;

            template <typename S, typename F>
            class snd_t_<S, F>::type
            {
            public:
                explicit type(stream_t<S, F>* strm) noexcept: stream_(strm) {}

            private:
                stream_t<S, F>* stream_;

                // TODO: if the predicate doesn't return a sender of boolean-testable report dependent_signature
                // clang-format off
                template <typename Env>
                friend join_sigs<
                    completion_signatures_of_t<next_result_t<S>, Env>,
                    completion_signatures<set_error_t(std::exception_ptr), set_stopped_t()>>
                tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept { return {}; }
                // clang-format on

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return ops_t<S, F, R>(self.stream_, static_cast<R&&>(recv));
                }
            };

            template <typename S, typename F>
            class stream_t_<S, F>::type
            {
            public:
                // clang-format off
                template <typename S2, typename F2>
                type(S2&& strm, F2&& func):
                    stream_(static_cast<S2&&>(strm)), func_(static_cast<F&&>(func)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S stream_;
                CLU_NO_UNIQUE_ADDRESS F func_;

                friend auto tag_invoke(next_t, type& self) noexcept { return snd_t<S, F>(&self); }
                friend auto tag_invoke(cleanup_t, type& self) noexcept { return cleanup(self.stream_); }

                friend S& get_stream(type& self) noexcept { return self.stream_; }
                friend F& get_func(type& self) noexcept { return self.func_; }
            };

            struct filter_t
            {
                template <stream S, typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, F&& func) 
                {
                    return stream_t<S, F>(static_cast<S&&>(strm), static_cast<F&&>(func));
                }

                template <typename F>
                CLU_STATIC_CALL_OPERATOR(auto)(F&& func) 
                {
                    return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
                }
            };
        } // namespace fltr

        // TODO: merge: stream<T>... -> stream<T>
        // TODO: join: stream<T>... -> stream<T>
        // TODO: debounce: stream<T>, time_scheduler, duration -> stream<T>
        // TODO: zip: stream<Ts>... -> stream<Ts...>
        // TODO: combine: stream<Ts>... -> stream<Ts...>
        // TODO: flatten: stream<stream<T>>, int? max_concurrency -> stream<T>

        namespace last
        {
            // TODO: implement last

            struct last_t
            {
                template <stream S>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& strm) 
                {
                }
                CLU_STATIC_CALL_OPERATOR(auto)()  noexcept { return make_piper(*this); }
            };
        } // namespace last

        // TODO: drop_while: stream<T>, (T -> sender<bool>) -> stream<T>
        // TODO: take_while: stream<T>, (T -> sender<bool>) -> stream<T>

        namespace vec
        {
            template <typename S, typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R>
            using ops_t = typename ops_t_<S, std::decay_t<R>>::type;

            template <typename S, typename R>
            struct next_recv_t_
            {
                class type;
            };

            template <typename S, typename R>
            using next_recv_t = typename next_recv_t_<S, R>::type;

            template <typename S, typename R>
            class next_recv_t_<S, R>::type
            {
            public:
                explicit type(ops_t<S, R>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, R>* ops_;

                template <typename Cpo, typename... Ts>
                void set(Cpo, Ts&&... args) noexcept;

                env_of_t<R> get_env() const noexcept;

                template <recvs::completion_cpo Cpo, typename... Ts>
                friend void tag_invoke(Cpo, type&& self, Ts&&... args) noexcept
                {
                    self.set(Cpo{}, static_cast<Ts&&>(args)...);
                }

                friend env_of_t<R> tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }
            };

            template <typename S, typename E>
            constexpr auto get_vector_type_impl() noexcept
            {
                if constexpr (similar_to<E, no_env>)
                    return type_tag<void>;
                else
                {
                    using next_type = next_result_t<S>;
                    if constexpr (requires { typename coro_utils::single_sender_value_type<next_type, E>; })
                    {
                        using value_type = coro_utils::single_sender_value_type<next_type, E>;
                        using env_alloc_t = call_result_t<get_allocator_t, E>;
                        using allocator_type =
                            typename std::allocator_traits<env_alloc_t>::template rebind_alloc<value_type>;
                        if constexpr (std::is_void_v<value_type>)
                            return type_tag<void>;
                        else
                            return type_tag<std::vector<value_type, allocator_type>>;
                    }
                    else
                        return type_tag<void>;
                }
            }
            template <typename S, typename E>
            using get_vector_type = typename decltype(get_vector_type_impl<S, E>())::type;

            template <typename S, typename E>
            constexpr auto get_comp_sigs_impl() noexcept
            {
                if constexpr (std::is_void_v<get_vector_type<S, E>>)
                    return dependent_completion_signatures<E>{};
                else
                    return make_completion_signatures<next_result_t<S>, E,
                        completion_signatures<set_value_t(get_vector_type<S, E>), set_error_t(std::exception_ptr)>,
                        meta::constant_q<completion_signatures<>>::fn>{};
            }

            template <typename S, typename R>
            class ops_t_<S, R>::type
            {
            public:
                // clang-format off
                template <typename S2, typename R2>
                type(S2&& strm, R2&& recv):
                    stream_(static_cast<S2&&>(strm)),
                    recv_(static_cast<R2&&>(recv)),
                    stoken_(get_stop_token(exec::get_env(recv_))),
                    result_(std::in_place_index<0>, get_allocator(exec::get_env(recv_))) {}
                // clang-format on

                env_of_t<R> get_env() const noexcept { return exec::get_env(recv_); }

                // Got a new value
                template <typename T>
                void next_done(set_value_t, T&& value) noexcept
                {
                    if (stoken_.stop_requested())
                    {
                        start_cleanup();
                        return;
                    }
                    std::get<0>(result_).push_back(static_cast<T&&>(value));
                    if (!start_next())
                        start_cleanup();
                }

                // Got an error, propagate it to the final receiver
                template <typename E>
                void next_done(set_error_t, E&& error) noexcept
                {
                    result_ = static_cast<E&&>(error);
                    start_cleanup();
                }

                // Got stopped, send the vector to the final receiver
                void next_done(set_stopped_t) noexcept { start_cleanup(); }

                void cleanup_done() noexcept
                {
                    std::visit(
                        [&]<typename Res>(Res&& res) noexcept
                        {
                            if constexpr (!std::is_same_v<Res, vector_type_t>) // error
                                set_error(static_cast<R&&>(recv_), static_cast<Res&&>(res));
                            else if (stoken_.stop_requested())
                                set_stopped(static_cast<R&&>(recv_));
                            else
                                set_value(static_cast<R&&>(recv_), static_cast<Res&&>(res));
                        },
                        std::move(result_));
                }

            private:
                using stop_token_t = stop_token_of_t<env_of_t<R>>;
                using vector_type_t = get_vector_type<S, env_of_t<R>>;

                CLU_NO_UNIQUE_ADDRESS S stream_;
                CLU_NO_UNIQUE_ADDRESS R recv_;

                stop_token_t stoken_;
                meta::unpack_invoke< //
                    meta::unique_l<meta::concatenate_l< //
                        type_list<vector_type_t, std::exception_ptr>,
                        error_types_of_t<next_result_t<S>, env_of_t<R>, type_list>>>,
                    meta::quote<std::variant>>
                    result_;
                ops_variant<std::monostate, //
                    connect_result_t<next_result_t<S>, next_recv_t<S, R>>,
                    connect_result_t<cleanup_result_t<S>, cleanup_recv_t<type>>>
                    current_ops_;

                bool start_next() noexcept
                {
                    try
                    {
                        start(current_ops_.template emplace<1>( //
                            copy_elider{[&] { return connect(next(stream_), next_recv_t<S, R>(this)); }}));
                    }
                    catch (...)
                    {
                        set_error(static_cast<R&&>(recv_), std::current_exception());
                        return false;
                    }
                    return true;
                }

                void start_cleanup() noexcept
                {
                    // If this throws std::terminate() will be called automatically
                    // since this function is noexcept
                    start(current_ops_.template emplace<2>( //
                        copy_elider{[&] { return connect(cleanup(stream_), cleanup_recv_t<type>(this)); }}));
                }

                friend void tag_invoke(start_t, type& self) noexcept { (void)self.start_next(); }
            };

            template <typename S, typename R>
            template <typename Cpo, typename... Ts>
            void next_recv_t_<S, R>::type::set(Cpo, Ts&&... args) noexcept
            {
                ops_->next_done(Cpo{}, static_cast<Ts&&>(args)...);
            }

            template <typename S, typename R>
            env_of_t<R> next_recv_t_<S, R>::type::get_env() const noexcept
            {
                return ops_->get_env();
            }

            template <typename S>
            struct snd_t_
            {
                class type;
            };

            template <typename S>
            using snd_t = typename snd_t_<std::decay_t<S>>::type;

            template <typename S>
            class snd_t_<S>::type
            {
            public:
                // clang-format off
                template <forwarding<S> S2>
                explicit type(S2&& strm): stream_(static_cast<S2&&>(strm)) {}
                // clang-format on

            private:
                CLU_NO_UNIQUE_ADDRESS S stream_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    return ops_t<S, R>(static_cast<S&&>(self.stream_), static_cast<R&&>(recv));
                }

                template <typename E>
                friend auto tag_invoke(get_completion_signatures_t, const type&, E&&) noexcept
                {
                    return get_comp_sigs_impl<S, E>();
                }
            };

            struct into_vector_t
            {
                template <stream S>
                CLU_STATIC_CALL_OPERATOR(auto)(S&& strm) 
                {
                    return snd_t<S>(static_cast<S&&>(strm));
                }
                CLU_STATIC_CALL_OPERATOR(auto)()  noexcept { return make_piper(*this); }
            };
        } // namespace vec

        // (the following two might not be suited for this header)
        // TODO: cache: stream<T> -> stream<T>
        // TODO: as_shared: stream<T> -> stream<T>
    } // namespace detail

    using detail::adpt_nxt::adapt_next_t;
    using detail::adpt_cln::adapt_cleanup_t;
    using detail::rdc::reduce_t;
    using detail::fltr::filter_t;
    using detail::vec::into_vector_t;

    inline constexpr adapt_next_t adapt_next{};
    inline constexpr adapt_cleanup_t adapt_cleanup{};
    inline constexpr reduce_t reduce{};
    inline constexpr filter_t filter{};
    inline constexpr into_vector_t into_vector{};
} // namespace clu::exec
