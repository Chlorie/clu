#pragma once

#include <vector>
#include <algorithm>

#include "assertion.h"
#include "concepts.h"
#include "integer_literals.h"

namespace clu
{
    namespace detail
    {
        template <typename... Ts>
        class event_holder
        {
        public:
            template <typename T, typename... Args>
            explicit event_holder(std::in_place_type_t<T>, Args&&... args)
            {
                if (use_sbo<T>)
                {
                    data_ptr_ = buffer_;
                    new (data_ptr_) T(static_cast<Args&&>(args)...);
                }
                else
                    data_ptr_ = new T(static_cast<Args&&>(args)...);
                vtbl_ = vtable{call_impl<T>, dtor_impl<T>, move_ctor_impl<T>};
            }

            ~event_holder() noexcept
            {
                if (data_ptr_)
                    vtbl_.dtor(data_ptr_);
            }

            event_holder(event_holder&& other) noexcept { this->move_from_other(other); }

            event_holder& operator=(event_holder&& other) noexcept
            {
                if (&other == this)
                    return *this;
                reset();
                this->move_from_other(other);
                return *this;
            }

            void operator()(Ts... args) { vtbl_.call(data_ptr_, static_cast<Ts&&>(args)...); }

        private:
            struct vtable
            {
                void (*call)(void* ptr, Ts... args);
                void (*dtor)(void* ptr) noexcept;
                void (*move_ctor)(void* src, void* dst) noexcept;
            };

            static constexpr std::size_t sbo_size = 2 * sizeof(void*);

            template <typename T>
            static constexpr bool use_sbo = alignof(T) <= alignof(void*) && sizeof(T) <= sbo_size;

            void* data_ptr_ = nullptr;
            vtable vtbl_{};
            alignas(void*) char buffer_[sbo_size];

            void move_from_other(event_holder& other) noexcept
            {
                if (other.data_ptr_ == other.buffer_) // other in SBO
                {
                    data_ptr_ = buffer_;
                    vtbl_ = other.vtbl_;
                    other.vtbl_.move_ctor(other.data_ptr_, data_ptr_);
                }
                else // other using heap
                {
                    data_ptr_ = std::exchange(other.data_ptr_, nullptr);
                    vtbl_ = std::exchange(other.vtbl_, {});
                }
            }

            template <typename T>
            static void call_impl(void* ptr, Ts... args)
            {
                (*static_cast<T*>(ptr))(static_cast<Ts&&>(args)...);
            }

            template <typename T>
            static void dtor_impl(void* ptr) noexcept
            {
                if constexpr (use_sbo<T>)
                    static_cast<T*>(ptr)->~T();
                else
                    delete static_cast<T*>(ptr);
            }

            template <typename T>
            static void move_ctor_impl(void* src, void* dst) noexcept
            {
                new (dst) T(static_cast<T&&>(*static_cast<T*>(src)));
            }

            void reset() noexcept
            {
                if (!data_ptr_)
                    return;
                vtbl_.dtor(data_ptr_);
                data_ptr_ = nullptr;
                vtbl_ = vtable{};
            }
        };
    } // namespace detail

    class event_subscription
    {
    private:
        template <typename...>
        friend class event;
        std::size_t index_ = 0;
        explicit event_subscription(const std::size_t index) noexcept: index_(index) {}
    };

    template <typename... Ts>
    class event
    {
    public:
        template <invocable_of<void, Ts...> F>
        event_subscription subscribe(F&& func)
        {
            std::size_t index = next_index();
            subs_.push_back(element{
                .index = index, //
                .holder = holder_t{std::in_place_type<std::decay_t<F>>, static_cast<F&&>(func)} //
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
                e.holder(args...);
        }

        // clang-format off
        template <invocable_of<void, Ts...> F>
        event_subscription operator+=(F&& func) { return this->subscribe(static_cast<F&&>(func)); }
        void operator-=(const event_subscription subscription) noexcept { unsubscribe(subscription); }
        // clang-format on

    private:
        using holder_t = detail::event_holder<Ts...>;
        struct element
        {
            std::size_t index;
            holder_t holder;
        };
        std::vector<element> subs_;

        std::size_t next_index() const noexcept { return subs_.empty() ? 0_uz : subs_.back().index + 1; }
    };
} // namespace clu
