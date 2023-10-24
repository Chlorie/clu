#include "clu/concurrency/hazard_pointer.h"

#include <unordered_set>
#include <algorithm>

namespace clu
{
    namespace detail
    {
#if __cpp_lib_hardware_interference_size >= 201703L
        struct alignas(std::hardware_destructive_interference_size) hp_impl
#else
        struct alignas(64) hp_impl // Best effort guess
#endif
        {
            hazard_pointer_domain* domain = nullptr;
            hp_impl* next = nullptr; // This is immutable once this hp is added into the domain's list
            hp_impl* next_avail = nullptr; // Another intrusive list in the domain for available hp-s
            std::atomic<const void*> hazard{}; // The protected object pointer
        };

        class hp_cache
        {
        public:
            ~hp_cache() noexcept
            {
                // TODO: optimize this with batched return
                for (std::size_t i = 0; i < size_; i++)
                    hazard_pointer_default_domain().return_available_hp(ptrs_[i]);
            }

            bool try_push_back(hp_impl* ptr) noexcept
            {
                if (size_ >= capacity)
                    return false;
                ptrs_[size_++] = ptr;
                return true;
            }

            hp_impl* try_pop_back() noexcept
            {
                if (size_ == 0)
                    return nullptr;
                return ptrs_[size_--];
            }

        private:
            static constexpr std::size_t capacity = 8;
            hp_impl* ptrs_[capacity]{};
            std::size_t size_ = 0;
        };

        hp_cache& get_cache()
        {
            thread_local hp_cache cache;
            return cache;
        }

        using clock = std::chrono::steady_clock;

        constexpr auto ns_since_epoch(const clock::time_point tp) noexcept
        {
            namespace chr = std::chrono;
            return chr::duration_cast<chr::nanoseconds>(tp.time_since_epoch()).count();
        }

        auto now() noexcept { return clock::now(); }
    } // namespace detail

    // ReSharper disable once CppPassValueParameterByConstReference
    hazard_pointer_domain::hazard_pointer_domain(const allocator alloc) noexcept: alloc_(alloc) {}

    hazard_pointer_domain::~hazard_pointer_domain() noexcept
    {
        // Folly leaks the pointers for the default domain, not necessary in this case.
        // The standard mandates that thread local objects (the cache) destructs before
        // static objects (the default domain).

        // Free all un-reclaimed objects
        for (auto& shard : retired_)
        {
            auto* node = shard.load(std::memory_order::acquire);
            while (node)
            {
                auto* next = node->next_;
                node->dtor_(*node);
                node = next;
            }
        }

        // Free all hazard pointers
        auto* head = head_.load(std::memory_order::acquire);
        while (head)
        {
            auto* next = head->next;
            alloc_.delete_object(head);
            head = next;
        }
    }

    detail::hp_impl* hazard_pointer_domain::allocate_hp()
    {
        auto* hp = alloc_.new_object<detail::hp_impl>();
        hp->domain = this;
        // Folly uses release for the success case, but shouldn't success >= failure?
        while (!head_.compare_exchange_weak(hp->next, hp, //
            std::memory_order::acq_rel, std::memory_order::acquire))
        {
        }
        hp_count_.fetch_add(1, std::memory_order::release);
        return hp;
    }

    detail::hp_impl* hazard_pointer_domain::acquire_hp()
    {
        if (auto* hp = try_acquire_available_hp())
            return hp;
        return allocate_hp();
    }

    // We need to lock the avail_ pointer, so try_acquire/return are unfortunately not wait-free.
    // But for the most cases, the hp would be directly retrieved from the thread local cache,
    // so this function won't be hit too often if the default domain is used.
    detail::hp_impl* hazard_pointer_domain::try_acquire_available_hp() noexcept
    {
        if (auto* hp = avail_.lock_and_load())
        {
            avail_.store_and_unlock(hp->next_avail);
            hp->next_avail = nullptr;
            return hp;
        }
        else
        {
            avail_.store_and_unlock(nullptr);
            return nullptr;
        }
    }

    void hazard_pointer_domain::return_available_hp(detail::hp_impl* hp) noexcept
    {
        // TODO: maybe there's no need to lock, just do a CAS would be fine
        auto* head = avail_.lock_and_load();
        hp->next_avail = head;
        avail_.store_and_unlock(hp);
    }

    void hazard_pointer_domain::retire_object(detail::hp_obj_node* node) noexcept
    {
        // TSan warns about not supporting C++ atomic thread fence, but actually
        // the bug only affects acquire/release fences, not sequentially consistent ones
        CLU_GCC_WNO_TSAN
        std::atomic_thread_fence(std::memory_order::seq_cst);
        CLU_GCC_RESTORE_WARNING
        const auto shard_idx =
            (std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(node)) >> ignored_bits) & shard_mask;
        auto& shard = retired_[shard_idx];
        while (!shard.compare_exchange_weak(node->next_, node, //
            std::memory_order::acq_rel, std::memory_order::acquire))
        {
        }
        retired_count_.fetch_add(1, std::memory_order::release);
        check_reclaim();
    }

    void hazard_pointer_domain::check_reclaim() noexcept
    {
        auto count = check_retired_count();
        if (count == 0 && (count = check_due()) == 0)
            return;
        reclaim(count); // TODO: asynchronous reclamation on other execution context?
    }

    std::int64_t hazard_pointer_domain::check_retired_count() noexcept
    {
        // Check if we've piled up too much garbage
        auto count = retired_count_.load(std::memory_order::acquire);
        while (std::cmp_greater_equal(count, //
            std::max(min_threshold, 2 * hp_count_.load(std::memory_order::acquire))))
            if (retired_count_.compare_exchange_weak(count, 0, //
                    std::memory_order::acq_rel, std::memory_order::acquire))
            {
                // Set next due time
                due_.store(detail::ns_since_epoch(detail::now() + sync_period), //
                    std::memory_order::release);
                return count;
            }
        return 0;
    }

    std::int64_t hazard_pointer_domain::check_due() noexcept
    {
        // Check if it's been too long since the last time we cleaned up our garbage
        const auto now = detail::ns_since_epoch(detail::now());
        const auto next_due = detail::ns_since_epoch(detail::now() + sync_period);
        if (auto due = due_.load(std::memory_order::acquire); std::cmp_less(now, due) ||
            !due_.compare_exchange_weak(due, next_due, std::memory_order::acq_rel, std::memory_order::relaxed))
            return 0;
        return retired_count_.exchange(0, std::memory_order::acq_rel);
    }

    void hazard_pointer_domain::reclaim(std::int64_t count) noexcept
    {
        while (true)
        {
            detail::hp_obj_node* lists[n_shards];
            bool done = true;
            bool empty = true;
            for (std::size_t i = 0; i < n_shards; i++)
            {
                if ((lists[i] = retired_[i].exchange(nullptr, std::memory_order::acq_rel)))
                    empty = false;
            }
            if (!empty)
            {
                CLU_GCC_WNO_TSAN
                std::atomic_thread_fence(std::memory_order::seq_cst);
                CLU_GCC_RESTORE_WARNING
                count -= reclaim_all_non_matching(lists, done);
            }
            if (count)
                retired_count_.fetch_add(count, std::memory_order::release);

            count = check_retired_count();
            if (count == 0 && done)
                break;
        }
    }

    std::int64_t hazard_pointer_domain::reclaim_all_non_matching(
        const std::span<detail::hp_obj_node* const> lists, bool& done) noexcept
    {
        // Load protected pointers
        std::pmr::unordered_set<const void*> hazards(alloc_);
        for (auto hp = head_.load(std::memory_order::acquire); hp; hp = hp->next)
            hazards.insert(hp->hazard.load(std::memory_order::acquire));

        // Reclaim
        std::int64_t count = 0;
        detail::hp_obj_node* unreclaimed = nullptr;
        detail::hp_obj_node* unreclaimed_tail = nullptr;
        for (auto* list : lists)
        {
            while (list)
            {
                auto* next = list->next_;
                if (hazards.contains(list))
                {
                    list->next_ = std::exchange(unreclaimed, list);
                    if (!unreclaimed_tail)
                        unreclaimed_tail = unreclaimed;
                }
                else
                {
                    count++;
                    list->dtor_(*list);
                }
                list = next;
            }
            if (!no_retired())
                done = false;
        }

        // Add unreclaimed nodes back into the 0th shard
        if (unreclaimed)
            while (!retired_[0].compare_exchange_weak(unreclaimed_tail->next_, unreclaimed, //
                std::memory_order::acq_rel, std::memory_order::acquire))
            {
            }
        return count;
    }

    bool hazard_pointer_domain::no_retired() const noexcept
    {
        return std::ranges::none_of(retired_,
            [](const std::atomic<detail::hp_obj_node*>& shard) noexcept
            { return shard.load(std::memory_order::acquire) != nullptr; });
    }

    hazard_pointer_domain& hazard_pointer_default_domain() noexcept
    {
        static hazard_pointer_domain domain;
        return domain;
    }

    hazard_pointer& hazard_pointer::operator=(hazard_pointer&& other) noexcept
    {
        if (&other != this)
        {
            destruct();
            hptr_ = std::exchange(other.hptr_, nullptr);
        }
        return *this;
    }

    void hazard_pointer::destruct() const noexcept
    {
        if (!hptr_)
            return;
        hptr_->hazard.store(nullptr, std::memory_order::release);
        // Default domain, try adding to the cache
        if (hptr_->domain == &hazard_pointer_default_domain())
            if (detail::get_cache().try_push_back(hptr_))
                return;
        hptr_->domain->return_available_hp(hptr_);
    }

    void hazard_pointer::reset_hazard(const detail::hp_obj_node* ptr) noexcept
    {
        hptr_->hazard.store(ptr, std::memory_order::release);
    }

    hazard_pointer make_hazard_pointer(hazard_pointer_domain& domain)
    {
        // Default domain, try getting a thread local hp
        if (&domain == &hazard_pointer_default_domain())
            if (auto* hp = detail::get_cache().try_pop_back()) [[likely]]
                return hazard_pointer(hp);
        return hazard_pointer(domain.acquire_hp());
    }
} // namespace clu
