#pragma once

#include "../execution/execution_traits.h"

namespace clu::async
{
    class mutex;

    namespace detail::mtx
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
            ops_t_(mutex* mut, R2&& recv):
                mut_(mut), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void tag_invoke(exec::start_t) noexcept
            {
                if (!start_ops(*mut_, *this)) // Acquired the lock synchronously
                    set();
            }

            void set() noexcept override { exec::set_value(static_cast<R&&>(recv_)); }

        private:
            mutex* mut_;
            CLU_NO_UNIQUE_ADDRESS R recv_;
        };

        template <typename R>
        using ops_t = ops_t_<std::remove_cvref_t<R>>;

        class snd_t
        {
        public:
            using is_sender = void;

            explicit snd_t(mutex* mut) noexcept: mut_(mut) {}

            // clang-format off
            static exec::completion_signatures<exec::set_value_t()> tag_invoke(
                exec::get_completion_signatures_t, auto&&) noexcept { return {}; }
            // clang-format on

            template <exec::receiver R>
            auto tag_invoke(exec::connect_t, R&& recv) const
            {
                return ops_t<R>(mut_, static_cast<R&&>(recv));
            }

        private:
            mutex* mut_;
        };
    } // namespace detail::mtx

    class mutex
    {
    public:
        mutex() noexcept = default;
        CLU_IMMOVABLE_TYPE(mutex);

        [[nodiscard]] bool try_lock() noexcept;
        [[nodiscard]] auto lock_async() noexcept { return detail::mtx::snd_t(this); }
        void unlock() noexcept;

    private:
        using ops_base = detail::mtx::ops_base;

        // Stores this when the mutex is not held, stores the latest waiting
        // operation state (or nullptr) otherwise
        std::atomic<void*> waiting_{this};
        ops_base* pending_ = nullptr;

        friend bool start_ops(mutex& self, ops_base& ops) noexcept;
    };
} // namespace clu::async
