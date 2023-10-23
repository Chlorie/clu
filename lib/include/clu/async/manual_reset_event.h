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
        class ops_t_ final : public ops_base
        {
        public:
            // clang-format off
            template <typename R2>
            ops_t_(manual_reset_event* ev, R2&& recv):
                ev_(ev), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set() noexcept override { exec::set_value(static_cast<R&&>(recv_)); }
            void tag_invoke(exec::start_t) noexcept;

        private:
            manual_reset_event* ev_;
            CLU_NO_UNIQUE_ADDRESS R recv_;
        };

        template <typename R>
        using ops_t = ops_t_<std::remove_cvref_t<R>>;

        class snd_t
        {
        public:
            using is_sender = void;

            explicit snd_t(manual_reset_event* ev) noexcept: ev_(ev) {}

            // clang-format off
            static exec::completion_signatures<exec::set_value_t()> tag_invoke(
                exec::get_completion_signatures_t, auto&&) noexcept { return {}; }

            template <exec::receiver R>
            auto tag_invoke(exec::connect_t, R&& recv) const { return ops_t<R>(ev_, static_cast<R&&>(recv)); }
            // clang-format on

        private:
            manual_reset_event* ev_;
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
        template <typename R>
        friend class detail::amre::ops_t_;

        // The tail stores this when the event is set, and stores a pointer to
        // the last inserted pending ops_base* if there is any, or nullptr otherwise.
        std::atomic<void*> tail_{nullptr};

        void start_ops(detail::amre::ops_base& ops);
    };

    template <typename R>
    void detail::amre::ops_t_<R>::tag_invoke(exec::start_t) noexcept
    {
        ev_->start_ops(*this);
    }
} // namespace clu::async
