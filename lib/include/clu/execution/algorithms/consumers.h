#pragma once

#include <optional>

#include "basic.h"
#include "../run_loop.h"

namespace clu::exec
{
    namespace detail
    {
        namespace start_det
        {
            template <typename S>
            struct ops_wrapper;

            template <typename S>
            struct recv_t
            {
                using is_receiver = void;

                ops_wrapper<S>* ptr = nullptr;

                template <typename... Ts>
                void tag_invoke(set_value_t, Ts&&...) && noexcept
                {
                    delete ptr;
                }

                template <typename E>
                void tag_invoke(set_error_t, E&&) && noexcept
                {
                    delete ptr;
                    std::terminate();
                }

                void tag_invoke(set_stopped_t) && noexcept { delete ptr; }
            };

            template <typename S>
            struct ops_wrapper
            {
                connect_result_t<S, recv_t<S>> op_state;
                explicit ops_wrapper(S&& snd): op_state(exec::connect(static_cast<S&&>(snd), recv_t<S>{.ptr = this})) {}
            };

            struct start_detached_t
            {
                template <sender S>
                constexpr CLU_STATIC_CALL_OPERATOR(void)(S&& snd)
                {
                    if constexpr ( //
                        requires {
                            requires tag_invocable<start_detached_t,
                                call_result_t<get_completion_scheduler_t<set_value_t>, S>, S>;
                        })
                    {
                        static_assert(std::is_void_v<tag_invoke_result_t<start_detached_t,
                                          call_result_t<get_completion_scheduler_t<set_value_t>, S>, S>>,
                            "start_detached should return void");
                        clu::tag_invoke(start_detached_t{}, //
                            exec::get_completion_scheduler<set_value_t>(snd), static_cast<S&&>(snd));
                    }
                    else if constexpr (tag_invocable<start_detached_t, S>)
                    {
                        static_assert(std::is_void_v<tag_invoke_result_t<start_detached_t, S>>,
                            "start_detached should return void");
                        clu::tag_invoke(start_detached_t{}, static_cast<S&&>(snd));
                    }
                    else
                        exec::start((new ops_wrapper<S>(static_cast<S&&>(snd)))->op_state);
                }
            };
        } // namespace start_det

        struct execute_t
        {
            template <scheduler S, std::invocable F>
            CLU_STATIC_CALL_OPERATOR(void)
            (S&& schd, F&& func)
            {
                if constexpr (tag_invocable<execute_t, S, F>)
                {
                    static_assert(std::is_void_v<tag_invoke_result_t<execute_t, S, F>>,
                        "customization for execute should return void");
                    clu::tag_invoke(execute_t{}, static_cast<S&&>(schd), static_cast<F&&>(func));
                }
                else
                {
                    start_det::start_detached_t{}( //
                        then(exec::schedule(static_cast<S&&>(schd)), static_cast<F&&>(func)));
                }
            }
        };
    } // namespace detail

    using detail::start_det::start_detached_t;
    using detail::execute_t;

    inline constexpr start_detached_t start_detached{};
    inline constexpr execute_t execute{};
} // namespace clu::exec

namespace clu::this_thread
{
    namespace detail::sync_wait
    {
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
            exec::sender_in<S, env_t> && 
            requires { typename result_t<S>; };
        // clang-format on

        template <typename S>
        class recv_t_
        {
        public:
            using is_receiver = void;

            recv_t_(run_loop* loop, variant_t<S>* ptr): loop_(loop), ptr_(ptr) {}

            env_t tag_invoke(get_env_t) const noexcept { return {loop_->get_scheduler()}; }

            template <typename... Ts>
                requires std::constructible_from<value_types_t<S>, Ts...>
            void tag_invoke(exec::set_value_t, Ts&&... args) && noexcept
            {
                try
                {
                    ptr_->template emplace<1>(static_cast<Ts&&>(args)...);
                    loop_->finish();
                }
                catch (...)
                {
                    exec::set_error(std::move(*this), std::current_exception());
                }
            }

            template <typename E>
            void tag_invoke(exec::set_error_t, E&& error) && noexcept
            {
                ptr_->template emplace<2>(exec::detail::make_exception_ptr(static_cast<E&&>(error)));
                loop_->finish();
            }

            void tag_invoke(exec::set_stopped_t) && noexcept
            {
                ptr_->template emplace<3>();
                loop_->finish();
            }

        private:
            run_loop* loop_;
            variant_t<S>* ptr_;
        };

        template <typename S>
        using recv_t = recv_t_<std::remove_cvref_t<S>>;

        struct sync_wait_t
        {
            template <sync_waitable_sender S>
            CLU_STATIC_CALL_OPERATOR(result_t<S>)
            (S&& snd)
            {
                if constexpr (requires {
                                  requires tag_invocable<sync_wait_t,
                                      exec::detail::completion_scheduler_of_t<exec::set_value_t, S>, S>;
                              })
                {
                    using comp_schd = exec::detail::completion_scheduler_of_t<exec::set_value_t, S>;
                    static_assert(std::is_same_v<tag_invoke_result_t<sync_wait_t, comp_schd, S>, result_t<S>>);
                    return clu::tag_invoke(sync_wait_t{},
                        exec::get_completion_scheduler<exec::set_value_t>(static_cast<S&&>(snd)),
                        static_cast<S&&>(snd));
                }
                else if constexpr (tag_invocable<sync_wait_t, S>)
                {
                    static_assert(std::is_same_v<tag_invoke_result_t<sync_wait_t, S>, result_t<S>>);
                    return clu::tag_invoke(sync_wait_t{}, static_cast<S&&>(snd));
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
        using var_result_t =
            std::optional<exec::value_types_of_t<into_var_snd_t<S>, env_t, single_type_t, single_type_t>>;

        struct sync_wait_with_variant_t
        {
            template <typename S>
                requires sync_waitable_sender<into_var_snd_t<S>>
            constexpr CLU_STATIC_CALL_OPERATOR(var_result_t<S>)(S&& snd)
            {
                if constexpr (requires {
                                  requires tag_invocable<sync_wait_with_variant_t,
                                      exec::detail::completion_scheduler_of_t<exec::set_value_t, S>, S>;
                              })
                {
                    using comp_schd = exec::detail::completion_scheduler_of_t<exec::set_value_t, S>;
                    static_assert(
                        std::is_same_v<tag_invoke_result_t<sync_wait_with_variant_t, comp_schd, S>, var_result_t<S>>);
                    return clu::tag_invoke(sync_wait_with_variant_t{},
                        exec::get_completion_scheduler<exec::set_value_t>(static_cast<S&&>(snd)),
                        static_cast<S&&>(snd));
                }
                else if constexpr (tag_invocable<sync_wait_with_variant_t, S>)
                {
                    static_assert(std::is_same_v<tag_invoke_result_t<sync_wait_with_variant_t, S>, var_result_t<S>>);
                    return clu::tag_invoke(sync_wait_with_variant_t{}, static_cast<S&&>(snd));
                }
                else
                {
                    // raw is optional<tuple<variant<tuple<...>...>>>
                    auto raw = sync_wait_t{}(exec::into_variant(static_cast<S&&>(snd)));
                    return raw ? var_result_t<S>(std::get<0>(*raw)) : var_result_t<S>();
                }
            }
        };
    } // namespace detail::sync_wait

    using detail::sync_wait::sync_wait_t;
    using detail::sync_wait::sync_wait_with_variant_t;

    inline constexpr sync_wait_t sync_wait{};
    inline constexpr sync_wait_with_variant_t sync_wait_with_variant{};
} // namespace clu::this_thread
