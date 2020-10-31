#pragma once

#include <tuple>

namespace clu
{
    namespace detail
    {
        template <typename Inv, typename... Ts>
        class partial_t
        {
        private:
            Inv invocable_;
            std::tuple<Ts...> args_;

        public:
            explicit constexpr partial_t(Inv&& invocable, Ts&&... args):
                invocable_(static_cast<Inv>(invocable)),
                args_(static_cast<Ts>(args)...) {}

            template <typename... Us> requires std::invocable<Inv&&, Us&&..., Ts&&...>
            constexpr decltype(auto) operator()(Us&&... args)
            {
                return std::apply([&]<typename... Bs>(Bs&&... bound_args)
                {
                    return std::invoke(static_cast<Inv&&>(invocable_),
                        std::forward<Us>(args)..., std::forward<Bs>(bound_args)...);
                }, args_);
            }

            template <typename U> requires std::invocable<Inv&&, U&&, Ts&&...>
            friend constexpr decltype(auto) operator|(U&& arg, partial_t&& part)
            {
                return std::move(part)(std::forward<U>(arg));
            }
        };
    }

    template <typename Inv, typename... Ts>
    constexpr auto partial(Inv&& invocable, Ts&&... args)
    {
        return detail::partial_t<Inv, Ts...>(
            std::forward<Inv>(invocable), std::forward<Ts>(args)...);
    }
}
