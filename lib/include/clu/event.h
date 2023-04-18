#pragma once

#include <vector>
#include <algorithm>

#include "assertion.h"
#include "concepts.h"
#include "integer_literals.h"
#include "function.h"

namespace clu
{
    class event_subscription
    {
    private:
        template <typename, typename...>
        friend class basic_event;
        std::size_t index_ = 0;
        explicit event_subscription(const std::size_t index) noexcept: index_(index) {}
    };

    template <typename Alloc, typename... Ts>
    class basic_event
    {
    public:
        using allocator_type = Alloc;

        basic_event() noexcept = default;
        basic_event(basic_event&&) = default;
        ~basic_event() noexcept = default;
        basic_event& operator=(basic_event&&) = default;

        explicit basic_event(const Alloc& alloc): subs_(alloc) {}
        basic_event(basic_event&& other, const Alloc& alloc): subs_(std::move(other.subs_), alloc) {}

        [[nodiscard]] Alloc get_allocator() const noexcept { return subs_.get_allocator(); }

        template <invocable_of<void, Ts...> F>
        event_subscription subscribe(F&& func)
        {
            std::size_t index = next_index();
            subs_.push_back(element{
                .index = index, //
                .func = static_cast<F&&>(func) //
            });
            return event_subscription(index);
        }

        void unsubscribe(const event_subscription subscription) noexcept
        {
            const auto iter = std::ranges::find(subs_, subscription.index_, &element::index);
            CLU_ASSERT(iter != subs_.end(), "Cannot find event subscription with the tag");
            subs_.erase(iter);
        }

        void operator()(Ts&&... args)
        {
            for (auto& e : subs_)
                e.func(args...);
        }

        // clang-format off
        template <invocable_of<void, Ts...> F>
        event_subscription operator+=(F&& func) { return this->subscribe(static_cast<F&&>(func)); }
        void operator-=(const event_subscription subscription) noexcept { unsubscribe(subscription); }
        // clang-format on

    private:
        struct element
        {
            std::size_t index;
            move_only_function<void(Ts...), Alloc> func;
        };
        std::vector<element, typename std::allocator_traits<Alloc>::template rebind_alloc<element>> subs_;

        std::size_t next_index() const noexcept { return subs_.empty() ? 0_uz : subs_.back().index + 1; }
    };

    template <typename... Ts>
    using event = basic_event<std::allocator<std::byte>, Ts...>;
} // namespace clu
