#pragma once

#include "execution_traits.h"
#include "utility.h"
#include "../piper.h"

namespace clu::exec
{
    namespace detail
    {
        template <typename T>
        using comp_sig_of_single = conditional_t<
            std::is_void_v<T>,
            set_value_t(),
            set_value_t(with_regular_void_t<T>)>;

        template <typename T, bool NoThrow = false>
        using comp_sigs_of_single = conditional_t<
            NoThrow,
            completion_signatures<conditional_t<
                std::is_void_v<T>,
                set_value_t(),
                set_value_t(with_regular_void_t<T>)>>,
            completion_signatures<conditional_t<
                std::is_void_v<T>,
                set_value_t(),
                set_value_t(with_regular_void_t<T>)>, set_error_t(std::exception_ptr)>>;

        template <typename F, typename... Args>
        using comp_sigs_of_inv = comp_sigs_of_single<
            std::invoke_result_t<F, Args...>,
            nothrow_invocable<F, Args...>>;

        namespace start_det
        {
            template <typename S>
            struct ops_wrapper;

            template <typename S>
            struct recv_
            {
                struct type;
            };

            template <typename S>
            struct recv_<S>::type
            {
                ops_wrapper<S>* ptr = nullptr;

                template <typename... Ts>
                friend void tag_invoke(set_value_t, type&& self, Ts&&...) noexcept { delete self.ptr; }

                template <typename = int>
                friend void tag_invoke(set_stopped_t, type&& self) noexcept { delete self.ptr; }

                template <typename E>
                friend void tag_invoke(set_error_t, type&& self, E&&) noexcept
                {
                    delete self.ptr;
                    std::terminate();
                }

                friend empty_env tag_invoke(get_env_t, const type&) noexcept { return {}; }
            };

            template <typename S>
            using recv_t = typename recv_<S>::type;

            template <typename S>
            struct ops_wrapper
            {
                connect_result_t<S, recv_t<S>> op_state;

                explicit ops_wrapper(S&& snd):
                    op_state(exec::connect(static_cast<S&&>(snd), recv_t<S>{ .ptr = this })) {}
            };

            struct start_detached_t
            {
                template <sender S>
                constexpr void operator()(S&& snd) const
                {
                    if constexpr (requires
                    {
                        requires tag_invocable<
                            start_detached_t,
                            call_result_t<get_completion_scheduler_t<set_value_t>, S>,
                            S>;
                    })
                    {
                        static_assert(std::is_void_v<tag_invoke_result_t<
                                start_detached_t,
                                call_result_t<get_completion_scheduler_t<set_value_t>, S>,
                                S>>,
                            "start_detached should return void");
                        clu::tag_invoke(
                            *this,
                            exec::get_completion_scheduler<set_value_t>(snd),
                            static_cast<S&&>(snd));
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
        }

        namespace just
        {
            template <typename R, typename Cpo, typename... Ts>
            struct ops_
            {
                class type;
            };

            template <typename R, typename Cpo, typename... Ts>
            class ops_<R, Cpo, Ts...>::type
            {
            public:
                template <typename Tup>
                type(R&& recv, Tup&& values):
                    recv_(static_cast<R&&>(recv)), values_(static_cast<Tup&&>(values)) {}

            private:
                [[no_unique_address]] R recv_;
                [[no_unique_address]] std::tuple<Ts...> values_;

                friend void tag_invoke(start_t, type& ops) noexcept
                {
                    std::apply([&](Ts&&... values)
                    {
                        Cpo{}(static_cast<type&&>(ops).recv_,
                            static_cast<Ts&&>(values)...);
                    }, static_cast<type&&>(ops).values_);
                }
            };

            template <typename R, typename Cpo, typename... Ts>
            using ops_t = typename ops_<R, Cpo, Ts...>::type;

            template <typename Cpo, typename... Ts>
            struct snd_
            {
                class type;
            };

            template <typename Cpo, typename... Ts>
            class snd_<Cpo, Ts...>::type
            {
            public:
                template <typename... Us>
                explicit type(Us&&... args): values_(static_cast<Us&&>(args)...) {}

            private:
                [[no_unique_address]] std::tuple<Ts...> values_;

                template <typename R, forwarding<type> Self>
                friend ops_t<R, Cpo, Ts...> tag_invoke(connect_t, Self&& snd, R&& recv)
                {
                    return { static_cast<R&&>(recv), static_cast<Self&&>(snd).values_ };
                }

                friend completion_signatures<Cpo(Ts ...)> tag_invoke(
                    get_completion_signatures_t, const type&, auto) noexcept { return {}; }
            };

            template <typename Cpo, typename... Ts>
            using snd_t = typename snd_<Cpo, Ts...>::type;

            struct just_t
            {
                template <typename... Ts>
                auto operator()(Ts&&... values) const
                {
                    return snd_t<set_value_t, std::decay_t<Ts>...>(
                        static_cast<Ts&&>(values)...);
                }
            };

            struct just_error_t
            {
                template <typename E>
                auto operator()(E&& error) const
                {
                    return snd_t<set_error_t, std::decay_t<E>>(
                        static_cast<E&&>(error));
                }
            };

            struct just_stopped_t
            {
                auto operator()() const noexcept { return snd_t<set_stopped_t>(); }
            };
        }

        namespace upon
        {
            // @formatter:off
            template <typename R, typename Cpo, typename F>
            struct recv_ {};
            template <typename R, typename F>
            struct recv_<R, set_value_t, F> { struct type; };
            template <typename R, typename F>
            struct recv_<R, set_error_t, F> { struct type; };
            template <typename R, typename F>
            struct recv_<R, set_stopped_t, F> { struct type; };
            // @formatter:on

            template <typename R, typename Cpo, typename F>
            using recv_t = typename recv_<R, Cpo, F>::type;

            template <typename R, typename Cpo, typename F>
            class recv_base : public receiver_adaptor<recv_t<R, Cpo, F>, R>
            {
            public:
                constexpr recv_base(R&& recv, F&& func):
                    receiver_adaptor<recv_t<R, Cpo, F>, R>(static_cast<R&&>(recv)),
                    func_(static_cast<F&&>(func)) {}

            protected:
                template <typename... Args>
                constexpr void set(Args&&... args) noexcept
                {
                    if constexpr (nothrow_invocable<F, Args...>)
                        this->set_impl(static_cast<Args&&>(args)...);
                    else
                        try
                        {
                            this->set_impl(static_cast<Args&&>(args)...);
                        }
                        catch (...)
                        {
                            exec::set_error(std::move(*this).base(),
                                std::current_exception());
                        }
                }

            private:
                F func_;

                template <typename... Args>
                constexpr void set_impl(Args&&... args) noexcept(nothrow_invocable<F, Args...>)
                {
                    if constexpr (std::is_void_v<std::invoke_result_t<F, Args...>>)
                    {
                        std::invoke(std::move(func_), static_cast<Args&&>(args)...);
                        exec::set_value(std::move(*this).base());
                    }
                    else
                    {
                        exec::set_value(std::move(*this).base(),
                            std::invoke(std::move(func_), static_cast<Args&&>(args)...));
                    }
                }
            };

            template <typename R, typename F>
            struct recv_<R, set_value_t, F>::type : recv_base<R, set_value_t, F>
            {
            public:
                using recv_base<R, set_value_t, F>::recv_base;

                template <typename... Args> requires
                    std::invocable<F, Args...> &&
                    receiver_of<R, comp_sigs_of_inv<F, Args...>>
                constexpr void set_value(Args&&... args) && noexcept
                {
                    this->set(static_cast<Args&&>(args)...);
                }
            };

            template <typename R, typename F>
            struct recv_<R, set_error_t, F>::type : recv_base<R, set_error_t, F>
            {
            public:
                using recv_base<R, set_error_t, F>::recv_base;

                template <typename E> requires
                    std::invocable<F, E> &&
                    receiver_of<R, comp_sigs_of_inv<F, E>>
                constexpr void set_error(E&& error) && noexcept
                {
                    this->set(static_cast<E&&>(error));
                }
            };

            template <typename R, typename F>
            struct recv_<R, set_stopped_t, F>::type : recv_base<R, set_stopped_t, F>
            {
            public:
                using recv_base<R, set_stopped_t, F>::recv_base;

                template <typename = int> requires
                    std::invocable<F> &&
                    receiver_of<R, comp_sigs_of_inv<F>>
                constexpr void set_stopped() && noexcept { this->set(); }
            };

            template <typename S, typename Cpo, typename F>
            struct snd_
            {
                class type;
            };

            struct make_sig_impl_base
            {
                template <typename... Ts>
                using value_sig = completion_signatures<set_value_t(Ts ...)>;
                template <typename E>
                using error_sig = completion_signatures<set_error_t(E)>;
                using stopped_sig = completion_signatures<set_stopped_t()>;
            };
            template <typename Cpo, typename F>
            struct make_sig_impl {};
            template <typename F>
            struct make_sig_impl<set_value_t, F> : make_sig_impl_base
            {
                template <typename... Ts>
                using value_sig = comp_sigs_of_inv<F, Ts...>;
            };
            template <typename F>
            struct make_sig_impl<set_error_t, F> : make_sig_impl_base
            {
                template <typename E>
                using error_sig = comp_sigs_of_inv<F, E>;
            };
            template <typename F>
            struct make_sig_impl<set_stopped_t, F> : make_sig_impl_base
            {
                using stopped_sig = comp_sigs_of_inv<F>;
            };

            template <typename S, typename Cpo, typename F>
            class snd_<S, Cpo, F>::type
            {
            public:
                template <typename S2, typename F2>
                constexpr type(S2&& snd, F2&& func):
                    snd_(static_cast<S2&&>(snd)), func_(static_cast<F2&&>(func)) {}

            private:
                [[no_unique_address]] S snd_;
                [[no_unique_address]] F func_;

                template <typename R>
                constexpr friend auto tag_invoke(connect_t, type&& snd, R&& recv)
                {
                    return exec::connect(
                        static_cast<type&&>(snd).snd_,
                        recv_t<R, Cpo, F>(static_cast<R&&>(recv), static_cast<type&&>(snd).func_)
                    );
                }

                using make_sig = make_sig_impl<Cpo, F>;

                template <typename Env>
                constexpr friend make_completion_signatures<
                    S, Env,
                    completion_signatures<>,
                    make_sig_impl<Cpo, F>::template value_sig,
                    make_sig_impl<Cpo, F>::template error_sig,
                    typename make_sig_impl<Cpo, F>::stopped_sig
                > tag_invoke(get_completion_signatures_t, type&&, Env) { return {}; }
            };

            template <typename S, typename Cpo, typename F>
            using snd_t = typename snd_<std::remove_cvref_t<S>, Cpo, std::decay_t<F>>::type;

            template <typename UponCpo, typename SetCpo>
            struct upon_t
            {
                template <sender S, typename F>
                constexpr auto operator()(S&& snd, F&& func) const
                {
                    if constexpr (requires { requires tag_invocable<UponCpo, completion_scheduler_of_t<SetCpo, S>, S, F>; })
                    {
                        static_assert(sender<tag_invoke_result_t<UponCpo, completion_scheduler_of_t<SetCpo, S>, S, F>>,
                            "customizations for then/upon_error/upon_stopped should return a sender");
                        return clu::tag_invoke(
                            UponCpo{},
                            exec::get_completion_scheduler<SetCpo>(static_cast<S&&>(snd)),
                            static_cast<S&&>(snd),
                            static_cast<F&&>(func)
                        );
                    }
                    else if constexpr (tag_invocable<UponCpo, S, F>)
                    {
                        static_assert(sender<tag_invoke_result_t<UponCpo, S, F>>,
                            "customizations for then/upon_error/upon_stopped should return a sender");
                        return clu::tag_invoke(UponCpo{}, static_cast<S&&>(snd), static_cast<F&&>(func));
                    }
                    else
                        return snd_t<S, SetCpo, F>(static_cast<S&&>(snd), static_cast<F&&>(func));
                }

                template <typename F>
                constexpr auto operator()(F&& func) const
                {
                    return clu::make_piper(clu::bind_back(UponCpo{}, static_cast<F&&>(func)));
                }
            };

            struct then_t : upon_t<then_t, set_value_t> {};
            struct upon_error_t : upon_t<upon_error_t, set_error_t> {};
            struct upon_stopped_t : upon_t<upon_stopped_t, set_stopped_t> {};
        }

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
        }
    }

    using detail::start_det::start_detached_t;
    inline constexpr start_detached_t start_detached{};
    using detail::just::just_t;
    using detail::just::just_error_t;
    using detail::just::just_stopped_t;
    inline constexpr just_t just{};
    inline constexpr just_error_t just_error{};
    inline constexpr just_stopped_t just_stopped{};
    using detail::upon::then_t;
    using detail::upon::upon_error_t;
    using detail::upon::upon_stopped_t;
    inline constexpr then_t then{};
    inline constexpr upon_error_t upon_error{};
    inline constexpr upon_stopped_t upon_stopped{};
    using detail::execute::execute_t;
    inline constexpr execute_t execute{};
}

namespace clu::this_thread
{
    inline struct sync_wait_t { } constexpr sync_wait{}; // TODO
}
