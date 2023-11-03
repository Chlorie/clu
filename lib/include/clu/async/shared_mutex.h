#pragma once

#include "../execution/execution_traits.h"
#include "../concurrency.h"

namespace clu::async
{
    class shared_mutex;

    namespace detail::shmtx
    {
        class ops_base
        {
        public:
            bool shared = false;
            ops_base* next = nullptr;

            explicit ops_base(const bool is_shared) noexcept: shared(is_shared) {}
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
            ops_t_(shared_mutex* mut, const bool shared, R2&& recv):
                ops_base(shared), mut_(mut), recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set() noexcept override { exec::set_value(static_cast<R&&>(recv_)); }

            void tag_invoke(exec::start_t) noexcept
            {
                if (!start_ops(*mut_, *this)) // Acquired the lock synchronously
                    set();
            }

        private:
            shared_mutex* mut_;
            CLU_NO_UNIQUE_ADDRESS R recv_;
        };

        template <typename R>
        using ops_t = ops_t_<std::remove_cvref_t<R>>;

        class snd_t
        {
        public:
            using is_sender = void;

            explicit snd_t(shared_mutex* mut, const bool shared) noexcept: mut_(mut), shared_(shared) {}

            // clang-format off
            static exec::completion_signatures<exec::set_value_t()> tag_invoke(
                exec::get_completion_signatures_t, auto&&) noexcept { return {}; }
            // clang-format on

            template <exec::receiver R>
            auto tag_invoke(exec::connect_t, R&& recv) const
            {
                return ops_t<R>(mut_, shared_, static_cast<R&&>(recv));
            }

        private:
            shared_mutex* mut_;
            bool shared_;
        };
    } // namespace detail::shmtx

    class shared_mutex
    {
    public:
        shared_mutex() noexcept = default;
        CLU_IMMOVABLE_TYPE(shared_mutex);

        [[nodiscard]] bool try_lock() noexcept;
        [[nodiscard]] auto lock_async() noexcept { return detail::shmtx::snd_t(this, false); }
        void unlock() noexcept;
        [[nodiscard]] bool try_lock_shared() noexcept;
        [[nodiscard]] auto lock_shared_async() noexcept { return detail::shmtx::snd_t(this, true); }
        void unlock_shared() noexcept;

    private:
        using ops_base = detail::shmtx::ops_base;
        static constexpr std::size_t unique_locked = static_cast<std::size_t>(-1);

        spinlock mut_;
        std::size_t shared_holder_ = 0;
        ops_base* waiting_ = nullptr;
        ops_base* pending_ = nullptr;

        friend bool start_ops(shared_mutex& self, ops_base& ops) noexcept;
        bool enqueue_unique(ops_base& ops) noexcept;
        bool enqueue_shared(ops_base& ops) noexcept;
        ops_base* get_resumption_ops() noexcept;
    };
} // namespace clu::async
