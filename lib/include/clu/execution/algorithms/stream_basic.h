#pragma once

#include "basic.h"

namespace clu::exec
{
    namespace detail
    {
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
                S stream_;
                F func_;

                friend auto tag_invoke(next_t, type& self) { return self.func_(next(self.stream_)); }
                friend auto tag_invoke(cleanup_t, type& self) noexcept { return cleanup(self.stream_); }
            };

            struct adapt_next_t
            {
                template <stream S, typename F>
                auto operator()(S&& strm, F&& func) const
                {
                    return stream_t<S, F>(static_cast<S&&>(strm), static_cast<F&&>(func));
                }

                template <typename F>
                auto operator()(F&& func) const
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
                auto operator()(S&& strm, F&& func) const
                {
                    return stream_t<S, F>(static_cast<S&&>(strm), static_cast<F&&>(func));
                }

                template <typename F>
                auto operator()(F&& func) const
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
            struct cleanup_recv_t_
            {
                class type;
            };

            template <typename S, typename R, typename T, typename F>
            using cleanup_recv_t = typename cleanup_recv_t_<S, R, T, F>::type;

            template <typename S, typename R, typename T, typename F>
            class cleanup_recv_t_<S, R, T, F>::type
            {
            public:
                explicit type(ops_t<S, R, T, F>* ops) noexcept: ops_(ops) {}

            private:
                ops_t<S, R, T, F>* ops_;

                friend void tag_invoke(set_value_t, type&& self) noexcept { self.ops_->cleanup_done(); }
                friend void tag_invoke(set_error_t, type, auto&&) noexcept { std::terminate(); }
                friend void tag_invoke(set_stopped_t, type) noexcept { std::terminate(); }
                friend empty_env tag_invoke(get_env_t, type) noexcept { return {}; }
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
                    decltype(auto) operator()(Args&&... args) const
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
                using cleanup_ops_t = connect_result_t<cleanup_result_t<S>, cleanup_recv_t<S, R, T, F>>;

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
                        copy_elider{[&] { return connect(cleanup(stream_), cleanup_recv_t<S, R, T, F>(this)); }}));
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
                friend join_sigs<completion_signatures<set_value_t(T)>,
                    make_completion_signatures<next_result_t<S>, Env, completion_signatures<>,
                        meta::constant_q<completion_signatures<>>::fn, default_set_error,
                        completion_signatures<set_stopped_t()>>>
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
                auto operator()(S&& strm, T&& initial, F&& func) const
                {
                    return snd_t<S, T, F>( //
                        static_cast<S&&>(strm), static_cast<T&&>(initial), static_cast<F&&>(func));
                }

                template <typename T, typename F>
                auto operator()(T&& initial, F&& func) const
                {
                    return clu::make_piper(clu::bind_back(*this, //
                        static_cast<T&&>(initial), static_cast<F&&>(func)));
                }
            };
        } // namespace rdc
    } // namespace detail

    using detail::adpt_nxt::adapt_next_t;
    using detail::adpt_cln::adapt_cleanup_t;
    using detail::rdc::reduce_t;

    inline constexpr adapt_next_t adapt_next{};
    inline constexpr adapt_cleanup_t adapt_cleanup{};
    inline constexpr reduce_t reduce{};

    inline struct for_each_t
    {
        template <stream S, typename F>
        auto operator()(S&& stream, F&& func) const
        {
            return reduce(static_cast<S&&>(stream), unit{},
                [f = static_cast<F&&>(func)]<typename... Ts>(unit, Ts&&... args) mutable
                {
                    (void)f(static_cast<Ts&&>(args)...);
                    return unit{};
                });
        }

        template <typename F>
        auto operator()(F&& func) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr for_each{};
} // namespace clu::exec
