#pragma once

#include "execution_traits.h"
#include "utility.h"
#include "../piper.h"

namespace clu::exec
{
    namespace detail
    {
        template <typename R, typename F>
        struct then_recv_
        {
            class type;
        };

        template <typename R, typename F>
        using then_recv = typename then_recv_<R, F>::type;

        template <typename R, typename F>
        class then_recv_<R, F>::type : public receiver_adaptor<type, R>
        {
        public:
            constexpr type(R&& recv, F&& func):
                receiver_adaptor<type, R>(static_cast<R&&>(recv)),
                func_(static_cast<F&&>(func)) {}

            template <typename... Args> requires
                std::invocable<F, Args...> &&
                receiver_of_single_type<R, std::invoke_result_t<F, Args...>>
            constexpr void set_value(Args&&... args) && noexcept
            {
                if constexpr (std::is_void_v<std::invoke_result_t<F, Args&&...>>)
                {
                    std::invoke(std::move(func_), static_cast<Args&&>(args)...);
                    exec::try_set_value(std::move(*this).base());
                }
                else
                {
                    exec::try_set_value(std::move(*this).base(),
                        std::invoke(std::move(func_), static_cast<Args&&>(args)...));
                }
            }

        private:
            F func_;
        };

        template <typename S, typename F>
        struct then_snd_
        {
            class type;
        };

        template <typename S, typename F>
        using then_snd = typename then_snd_<S, F>::type;

        template <typename T>
        using single_type_value_sig = conditional_t<
            std::is_void_v<T>,
            set_value_t(),
            set_value_t(with_regular_void_t<T>)
        >;

        template <typename S, typename F>
        class then_snd_<S, F>::type
        {
        public:
            template <typename S2, typename F2>
            constexpr type(S2&& snd, F2&& func):
                snd_(static_cast<S2&&>(snd)), func_(static_cast<F2&&>(func)) {}

            // private:
            [[no_unique_address]] S snd_;
            [[no_unique_address]] F func_;

            template <receiver R>
            using recv_t = then_recv<std::remove_cvref_t<R>, F>;

            template <typename... Ts> requires std::invocable<F, Ts...>
            using value_sig = single_type_value_sig<std::invoke_result_t<F, Ts...>>;

            template <receiver R>
                requires sender_to<S, recv_t<R>>
            constexpr friend auto tag_invoke(connect_t, type&& snd, R&& recv)
            {
                return exec::connect(
                    static_cast<type&&>(snd).snd_,
                    recv_t<R>(static_cast<R&&>(recv), static_cast<type&&>(snd).func_)
                );
            }

            template <typename Env>
            constexpr friend make_completion_signatures<
                S, Env,
                completion_signatures<set_error_t(std::exception_ptr)>,
                value_sig
            > tag_invoke(get_completion_signatures_t, type&&, Env) { return {}; }
        };
    }

    inline struct then_t
    {
        template <sender S, typename F>
        constexpr sender auto operator()(S&& snd, F&& func) const
        {
            if constexpr (requires
            {
                {
                    clu::tag_invoke(
                        *this,
                        exec::get_completion_scheduler<set_value_t>(static_cast<S&&>(snd)),
                        static_cast<S&&>(snd),
                        static_cast<F&&>(func)
                    )
                } -> sender;
            })
                clu::tag_invoke(
                    *this,
                    exec::get_completion_scheduler<set_value_t>(static_cast<S&&>(snd)),
                    static_cast<S&&>(snd),
                    static_cast<F&&>(func)
                );
            else if constexpr (requires
            {
                {
                    clu::tag_invoke(*this, static_cast<S&&>(snd), static_cast<F&&>(func))
                } -> sender;
            })
                clu::tag_invoke(*this, static_cast<S&&>(snd), static_cast<F&&>(func));
            else
                return detail::then_snd<std::remove_cvref_t<S>, std::decay_t<F>>(
                    static_cast<S&&>(snd), static_cast<F&&>(func));
        }

        template <typename F>
        constexpr auto operator()(F&& func) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr then{};
}
