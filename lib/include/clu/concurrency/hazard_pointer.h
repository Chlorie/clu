#pragma once

// Rough implementation of P1121R3, using Folly as a reference
// Hazard Pointers: Proposed Interface and Wording for Concurrency TS 2

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <atomic>
#include <span>
#include <utility>

#include "../macros.h"
#include "locked_ptr.h"

namespace clu
{
    class hazard_pointer;
    class hazard_pointer_domain;
    template <typename T, typename D>
    class hazard_pointer_obj_base;

    namespace detail
    {
        struct hp_impl;
        class hp_cache;

        class hp_obj_node
        {
        protected:
            friend hazard_pointer_domain;
            hp_obj_node* next_ = nullptr;
            void (*dtor_)(hp_obj_node& self) noexcept = nullptr;
        };
    } // namespace detail

    class hazard_pointer_domain
    {
    public:
        using allocator = std::pmr::polymorphic_allocator<>;

        CLU_IMMOVABLE_TYPE(hazard_pointer_domain);
        hazard_pointer_domain() noexcept: hazard_pointer_domain(allocator{}) {}
        explicit hazard_pointer_domain(allocator alloc) noexcept;
        ~hazard_pointer_domain() noexcept;
        void clean_up() noexcept { reclaim(0); }

    private:
        friend detail::hp_impl;
        friend detail::hp_cache;
        friend hazard_pointer;
        template <typename T, typename D>
        friend class hazard_pointer_obj_base;

        static constexpr std::size_t min_threshold = 1000; // min amount of retired object before reclaiming
        static constexpr std::size_t multiplier = 2; // multiplier * hp_count_ is another min threshold
        static constexpr auto sync_period = std::chrono::seconds(2);

        static constexpr std::size_t ignored_bits = 8; // shift the hash of ptrs by this amount
        static constexpr std::size_t n_shards = 8;
        static constexpr std::size_t shard_mask = n_shards - 1;
        static_assert((n_shards & shard_mask) == 0, "n_shards should be a power of 2");

        allocator alloc_;
        std::atomic<detail::hp_impl*> head_;
        locked_ptr<detail::hp_impl> avail_;
        std::atomic_uint64_t due_;
        std::atomic_int64_t retired_count_;
        std::atomic_size_t hp_count_;
        std::atomic<detail::hp_obj_node*> retired_[n_shards];

        detail::hp_impl* allocate_hp();
        detail::hp_impl* acquire_hp();
        detail::hp_impl* try_acquire_available_hp() noexcept;
        void return_available_hp(detail::hp_impl* hp) noexcept;
        void retire_object(detail::hp_obj_node* node) noexcept;
        void check_reclaim() noexcept;
        std::int64_t check_retired_count() noexcept;
        std::int64_t check_due() noexcept;
        void reclaim(std::int64_t count) noexcept;
        std::int64_t reclaim_all_non_matching(std::span<detail::hp_obj_node* const> lists, bool& done) noexcept;
        bool no_retired() const noexcept;

        friend hazard_pointer make_hazard_pointer(hazard_pointer_domain& domain);
    };

    hazard_pointer_domain& hazard_pointer_default_domain() noexcept;

    inline void hazard_pointer_clean_up(hazard_pointer_domain& domain = hazard_pointer_default_domain()) noexcept
    {
        domain.clean_up();
    }

    template <typename T, typename D = std::default_delete<T>>
    class hazard_pointer_obj_base : public detail::hp_obj_node
    {
    public:
        void retire(D deleter = D{}, hazard_pointer_domain& domain = hazard_pointer_default_domain()) noexcept
        {
            deleter_ = std::move(deleter);
            dtor_ = [](hp_obj_node& self) noexcept
            {
                auto& base = static_cast<hazard_pointer_obj_base&>(self);
                auto& value = static_cast<T&>(self);
                base.deleter_(std::addressof(value));
            };
            domain.retire_object(this);
        }

        void retire(hazard_pointer_domain& domain = hazard_pointer_default_domain()) noexcept
        {
            this->retire(D(), domain);
        }

    protected:
        hazard_pointer_obj_base() noexcept = default;

    private:
        CLU_NO_UNIQUE_ADDRESS D deleter_;
    };

    class hazard_pointer
    {
    public:
        hazard_pointer() noexcept = default;
        hazard_pointer(hazard_pointer&& other) noexcept: hptr_(std::exchange(other.hptr_, nullptr)) {}
        hazard_pointer& operator=(hazard_pointer&& other) noexcept;
        ~hazard_pointer() noexcept { destruct(); }

        [[nodiscard]] bool empty() const noexcept { return !hptr_; }

        template <typename T>
        T* protect(const std::atomic<T*>& src) noexcept
        {
            T* ptr = src.load(std::memory_order::relaxed);
            while (!this->try_protect(ptr, src)) {} // Loop until we got the pointer
            return ptr;
        }

        template <typename T>
        bool try_protect(T*& ptr, const std::atomic<T*>& src) noexcept
        {
            T* p = ptr;
            this->reset_protection(p);
            std::atomic_thread_fence(std::memory_order::seq_cst);
            ptr = src.load(std::memory_order::acquire);
            if (p == ptr) [[likely]] // No one modified the atomic pointer while we're trying to protect it
                return true;
            this->reset_protection(); // We "protected" some garbage, throw it away
            return false;
        }

        template <typename T>
        void reset_protection(const T* ptr) noexcept
        {
            reset_hazard(static_cast<const detail::hp_obj_node*>(ptr));
        }

        void reset_protection(std::nullptr_t = nullptr) noexcept { reset_hazard(nullptr); }

        void swap(hazard_pointer& other) noexcept { std::swap(hptr_, other.hptr_); }
        friend void swap(hazard_pointer& lhs, hazard_pointer& rhs) noexcept { lhs.swap(rhs); }

        friend hazard_pointer make_hazard_pointer(hazard_pointer_domain& domain);

    private:
        detail::hp_impl* hptr_ = nullptr;

        explicit hazard_pointer(detail::hp_impl* hptr) noexcept: hptr_(hptr) {}
        void destruct() const noexcept;
        void reset_hazard(const detail::hp_obj_node* ptr) noexcept;
    };

    hazard_pointer make_hazard_pointer(hazard_pointer_domain& domain = hazard_pointer_default_domain());
} // namespace clu
