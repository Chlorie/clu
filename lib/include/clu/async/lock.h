#pragma once

#include <utility>
#include <mutex>

#include "../concepts.h"
#include "../execution/execution_traits.h"
#include "../execution/algorithms/basic.h"

namespace clu::async
{
    // clang-format off
	template <typename L>
    concept basic_lockable = requires(L m)
    {
        { m.lock_async() } -> exec::sender;
        { m.unlock() } noexcept;
    };

    template <typename L>
    concept lockable =
        basic_lockable<L> &&
        requires(L m)
        {
            { m.try_lock() } -> boolean_testable;
        };

    template <typename L>
    concept shared_lockable = requires(L m)
    {
        { m.lock_shared_async() } -> exec::sender;
        { m.try_lock_shared() } -> boolean_testable;
        { m.unlock_shared() } noexcept;
    };

    template <typename M>
    concept mutex_like =
        std::default_initializable<M> &&
        std::destructible<M> &&
        (!std::copyable<M>) &&
        (!std::movable<M>) &&
        lockable<M>;

    template <typename M>
    concept shared_mutex_like =
        mutex_like<M> &&
        shared_lockable<M>;
    // clang-format on

    namespace detail
    {
        template <typename L, bool Shared>
        class raii_lock_base
        {
        public:
            using mutex_type = L;

            constexpr raii_lock_base() noexcept = default;
            CLU_NON_COPYABLE_TYPE(raii_lock_base);

            // clang-format off
            constexpr raii_lock_base(raii_lock_base&& other) noexcept:
                ptr_(std::exchange(other.ptr_, nullptr)), locked_(std::exchange(other.locked_, false)) {}
            // clang-format on

            raii_lock_base(L& mutex, std::defer_lock_t) noexcept: ptr_(std::addressof(mutex)) {}
            raii_lock_base(L& mutex, std::adopt_lock_t) noexcept: ptr_(std::addressof(mutex)), locked_(true) {}

            ~raii_lock_base() noexcept
            {
                if (locked_)
                    unchecked_unlock();
            }

            raii_lock_base& operator=(raii_lock_base&& other) noexcept
            {
                if (&other == this)
                    return *this;
                if (locked_)
                    unchecked_unlock();
                ptr_ = std::exchange(other.ptr_, nullptr);
                locked_ = std::exchange(other.locked_, false);
                return *this;
            }

            void unlock() noexcept
            {
                if (!locked_)
                    throw std::system_error(std::make_error_code(std::errc::operation_not_permitted));
                unchecked_unlock();
            }

            void swap(raii_lock_base& other) noexcept
            {
                std::swap(ptr_, other.ptr_);
                std::swap(locked_, other.locked_);
            }

            friend void swap(raii_lock_base& lhs, raii_lock_base& rhs) noexcept { lhs.swap(rhs); }

            L* release() noexcept
            {
                locked_ = false;
                return std::exchange(ptr_, nullptr);
            }

            [[nodiscard]] constexpr L* mutex() const noexcept { return ptr_; }
            [[nodiscard]] constexpr bool owns_lock() const noexcept { return locked_; }
            [[nodiscard]] constexpr explicit operator bool() const noexcept { return locked_; }

        protected:
            L* ptr_ = nullptr;
            bool locked_ = false;

            constexpr raii_lock_base(L* ptr, const bool locked) noexcept: ptr_(ptr), locked_(locked) {}

            static void throw_system_error(const std::errc e) { throw std::system_error(std::make_error_code(e)); }
            void check_lockable()
            {
                if (!ptr_)
                    throw_system_error(std::errc::operation_not_permitted);
                if (locked_)
                    throw_system_error(std::errc::resource_deadlock_would_occur);
            }

            void unchecked_unlock() noexcept
            {
                if constexpr (Shared)
                    ptr_->unlock_shared();
                else
                    ptr_->unlock();
                locked_ = false;
            }
        };
    } // namespace detail

    template <basic_lockable L>
    class [[nodiscard]] unique_lock : public detail::raii_lock_base<L, false>
    {
    private:
        using base = detail::raii_lock_base<L, false>;

    public:
        using mutex_type = L;
        using base::base;

        // clang-format off
        unique_lock(L& mutex, std::try_to_lock_t) requires lockable<L>:
            base(std::addressof(mutex), this->ptr_->try_lock()) {}
        // clang-format on

        void swap(unique_lock& other) noexcept { base::swap(other); }
        friend void swap(unique_lock& lhs, unique_lock& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] auto lock_async()
        {
            this->check_lockable();
            return this->ptr_->lock_async() | exec::then([this] { this->locked_ = true; });
        }
        bool try_lock()
            requires lockable<L>
        {
            return (this->locked_ = this->ptr_->try_lock());
        }
    };

    template <shared_lockable L>
    class [[nodiscard]] shared_lock : public detail::raii_lock_base<L, true>
    {
    private:
        using base = detail::raii_lock_base<L, true>;

    public:
        using mutex_type = L;
        using base::base;

        shared_lock(L& mutex, std::try_to_lock_t): base(std::addressof(mutex), this->ptr_->try_lock_shared()) {}

        void swap(shared_lock& other) noexcept { base::swap(other); }
        friend void swap(shared_lock& lhs, shared_lock& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] auto lock_async()
        {
            this->check_lockable();
            return this->ptr_->lock_shared_async() | exec::then([this] { this->locked_ = true; });
        }
        bool try_lock() { return (this->locked_ = this->ptr_->try_lock_shared()); }
    };

    template <basic_lockable L>
    [[nodiscard]] auto lock_scoped_async(L& mutex)
    {
        return mutex.lock_async() //
            | exec::then([&]() noexcept { return unique_lock<L>(mutex, std::adopt_lock); });
    }

    template <shared_lockable L>
    [[nodiscard]] auto lock_shared_scoped_async(L& mutex)
    {
        return mutex.lock_shared_async() //
            | exec::then([&]() noexcept { return shared_lock<L>(mutex, std::adopt_lock); });
    }
} // namespace clu::async
