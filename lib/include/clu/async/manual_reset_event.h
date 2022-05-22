#pragma once

#include "../execution/execution_traits.h"

namespace clu::async
{
    class manual_reset_event;

    namespace detail::amre
    {
        class ops_base
        {
        public:
            ops_base* next = nullptr;

            ops_base() noexcept = default;
            CLU_IMMOVABLE_TYPE(ops_base);
            virtual void set() noexcept = 0;

        protected:
            ~ops_base() noexcept = default;
        };

        template <typename R>
        struct ops_t_
        {
            class type;
        };

        template <typename R>
        using ops_t = typename ops_t_<std::decay_t<R>>::type;

        template <typename R>
        class ops_t_<R>::type final : public ops_base
        {
        public:
            // clang-format off
            template <typename R2>
            type(manual_reset_event* ev, R2&& recv):
                ev_(ev), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set() noexcept override { exec::set_value(static_cast<R&&>(recv_)); }

        private:
            manual_reset_event* ev_;
            CLU_NO_UNIQUE_ADDRESS R recv_;

            friend void tag_invoke(exec::start_t, type& self) noexcept { start_ops(*self.ev_, self); }
        };

        class snd_t
        {
        public:
            explicit snd_t(manual_reset_event* ev) noexcept: ev_(ev) {}

        private:
            manual_reset_event* ev_;

            // clang-format off
            friend exec::completion_signatures<exec::set_value_t()> tag_invoke(
                exec::get_completion_signatures_t, snd_t, auto&&) noexcept { return {}; }
            // clang-format on

            template <typename R>
            friend auto tag_invoke(exec::connect_t, const snd_t self, R&& recv)
            {
                return ops_t<R>(self.ev_, static_cast<R&&>(recv));
            }
        };
    } // namespace detail::amre

    class manual_reset_event
    {
    public:
        explicit manual_reset_event(const bool start_set = false) noexcept: tail_(start_set ? this : nullptr) {}
        manual_reset_event(manual_reset_event&&) = delete;
        manual_reset_event(const manual_reset_event&) = delete;
        ~manual_reset_event() noexcept = default;

        void set() noexcept;
        bool ready() const noexcept { return tail_.load(std::memory_order::acquire) == this; }
        void reset() noexcept;
        [[nodiscard]] auto wait_async() noexcept { return detail::amre::snd_t(this); }

    private:
        // The tail stores this when the event is set, and stores a pointer to
        // the last inserted pending ops_base* if there is any, or nullptr otherwise.
        std::atomic<void*> tail_{nullptr};

        friend void start_ops(manual_reset_event& self, detail::amre::ops_base& ops);
    };
} // namespace clu::async
