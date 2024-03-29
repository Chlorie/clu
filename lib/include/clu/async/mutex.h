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
            type(mutex* mut, R2&& recv):
                mut_(mut), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set() noexcept override { exec::set_value(static_cast<R&&>(recv_)); }

        private:
            mutex* mut_;
            CLU_NO_UNIQUE_ADDRESS R recv_;

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                if (!start_ops(*self.mut_, self)) // Acquired the lock synchronously
                    self.set();
            }
        };

        class snd_t
        {
        public:
            using is_sender = void;

            explicit snd_t(mutex* mut) noexcept: mut_(mut) {}

        private:
            mutex* mut_;

            // clang-format off
            friend exec::completion_signatures<exec::set_value_t()> tag_invoke(
                exec::get_completion_signatures_t, snd_t, auto&&) noexcept { return {}; }
            // clang-format on

            template <typename R>
            friend auto tag_invoke(exec::connect_t, const snd_t self, R&& recv)
            {
                return ops_t<R>(self.mut_, static_cast<R&&>(recv));
            }
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
