#pragma once

#include "execution_traits.h"
#include "execution"

namespace clu::exec
{
    inline struct start_detached_t
    {
        template <sender S>
        constexpr void operator()(S&& snd) const
        {
            if constexpr (requires
            {
                {
                    clu::tag_invoke(
                        *this,
                        exec::get_completion_scheduler<set_value_t>(static_cast<S&&>(snd)),
                        static_cast<S&&>(snd)
                    )
                } -> std::same_as<void>;
            })
                clu::tag_invoke(
                    *this,
                    exec::get_completion_scheduler<set_value_t>(static_cast<S&&>(snd)),
                    static_cast<S&&>(snd)
                );
            else if constexpr (requires
            {
                {
                    clu::tag_invoke(*this, static_cast<S&&>(snd))
                } -> std::same_as<void>;
            })
                clu::tag_invoke(*this, static_cast<S&&>(snd));
            else
                exec::start((new op_state_wrapper<S>(static_cast<S&&>(snd)))->op_state);
        }

        // private:
        template <typename S> struct op_state_wrapper;

        template <typename S>
        struct recv_
        {
            struct type
            {
                op_state_wrapper<S>* ptr = nullptr;

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
            };
        };

        template <typename S> using recv_t = typename recv_<S>::type;

        template <typename S>
        struct op_state_wrapper
        {
            connect_result_t<S, recv_t<S>> op_state;

            explicit op_state_wrapper(S&& snd):
                op_state(exec::connect(static_cast<S&&>(snd), recv_t<S>{ .ptr = this })) {}
        };
    } constexpr start_detached{};
}

namespace clu::this_thread
{
    inline struct sync_wait_t { } constexpr sync_wait{};
}
