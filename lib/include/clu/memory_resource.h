#pragma once

#include <memory_resource>

#include "scope.h"

namespace clu
{
    namespace pmr
    {
        struct default_tag
        {
        };

#define CLU_PMR_PROP_CONCEPT(trait)                                                                                    \
    template <typename Traits>                                                                                         \
    concept trait = Traits::trait::value

        CLU_PMR_PROP_CONCEPT(propagate_on_container_copy_assignment);
        CLU_PMR_PROP_CONCEPT(propagate_on_container_move_assignment);
        CLU_PMR_PROP_CONCEPT(propagate_on_container_swap);
        CLU_PMR_PROP_CONCEPT(propagate_on_container_copy_construction);

#undef CLU_PMR_PROP_CONCEPT

        struct non_propagating_traits
        {
        };

        struct propagating_traits
        {
            using propagate_on_container_copy_assignment = std::true_type;
            using propagate_on_container_move_assignment = std::true_type;
            using propagate_on_container_swap = std::true_type;
            using propagate_on_container_copy_construction = std::true_type;
        };
    } // namespace pmr

    template <typename T = std::byte, typename Traits = pmr::non_propagating_traits, typename Tag = pmr::default_tag>
    class polymorphic_allocator
    {
    public:
        using value_type = T;
#define CLU_PMR_PROP_TYPEDEF(trait) using trait = std::bool_constant<pmr::trait<Traits>>
        CLU_PMR_PROP_TYPEDEF(propagate_on_container_copy_assignment);
        CLU_PMR_PROP_TYPEDEF(propagate_on_container_move_assignment);
        CLU_PMR_PROP_TYPEDEF(propagate_on_container_swap);
#undef CLU_PMR_PROP_TYPEDEF

        polymorphic_allocator() noexcept: polymorphic_allocator(std::pmr::get_default_resource()) {}
        explicit(false) polymorphic_allocator(std::pmr::memory_resource* res) noexcept: res_(res) {}
        polymorphic_allocator(const polymorphic_allocator&) noexcept = default;

        template <typename U>
        explicit(false) polymorphic_allocator(const polymorphic_allocator<U, Traits, Tag>& other) noexcept: //
            res_(other.resource())
        {
        }

        [[nodiscard]] T* allocate(const std::size_t n)
        {
            return static_cast<T*>(res_->allocate(n * sizeof(T), alignof(T)));
        }

        void deallocate(T* ptr, const std::size_t n) noexcept { res_->deallocate(ptr, n * sizeof(T), alignof(T)); }

        template <typename U, typename... Args>
        void construct(U* ptr, Args&&... args)
        {
            std::uninitialized_construct_using_allocator(ptr, *this, static_cast<Args&&>(args)...);
        }

        [[nodiscard]] void* allocate_bytes(
            const std::size_t nbytes, const std::size_t alignment = alignof(std::max_align_t))
        {
            return res_->allocate(nbytes, alignment);
        }

        void deallocate_bytes(
            void* p, const std::size_t nbytes, const std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            res_->deallocate(p, nbytes, alignment);
        }

        template <typename U>
        [[nodiscard]] U* allocate_object(const std::size_t n = 1)
        {
            if (std::numeric_limits<std::size_t>::max() / sizeof(U) < n)
                throw std::bad_array_new_length{};
            return static_cast<U*>(allocate_bytes(n * sizeof(U), alignof(U)));
        }

        template <typename U>
        void deallocate_object(U* ptr, const std::size_t n = 1)
        {
            this->deallocate_bytes(ptr, n * sizeof(U), alignof(U));
        }

        template <typename U, typename... Args>
        [[nodiscard]] U* new_object(Args&&... args)
        {
            U* ptr = allocate_object<U>();
            scope_fail guard{[&] { this->deallocate_object(ptr); }};
            this->construct(ptr, static_cast<Args&&>(args)...);
            return ptr;
        }

        template <typename U>
        void delete_object(U* ptr) noexcept
        {
            std::allocator_traits<polymorphic_allocator<U, Traits, Tag>>::destroy(*this, ptr);
            this->deallocate_object(ptr);
        }

        [[nodiscard]] polymorphic_allocator select_on_container_copy_construction() const noexcept
        {
            if constexpr (pmr::propagate_on_container_copy_construction<Traits>)
                return *this;
            else
                return {};
        }

        [[nodiscard]] std::pmr::memory_resource* resource() const noexcept { return res_; }

        [[nodiscard]] friend bool operator==(
            const polymorphic_allocator& lhs, const polymorphic_allocator& rhs) noexcept = default;

    private:
        std::pmr::memory_resource* res_ = nullptr;
    };

    template <typename T, typename U, typename Traits, typename Tag>
    [[nodiscard]] bool operator==( //
        const polymorphic_allocator<T, Traits, Tag>& lhs, //
        const polymorphic_allocator<U, Traits, Tag>& rhs) noexcept
    {
        return *lhs.resource() == *rhs.resource();
    }

    template <typename T = std::byte, typename Tag = pmr::default_tag>
    using non_propagating_polymorphic_allocator = polymorphic_allocator<T, pmr::non_propagating_traits, Tag>;
    template <typename T = std::byte, typename Tag = pmr::default_tag>
    using propagating_polymorphic_allocator = polymorphic_allocator<T, pmr::propagating_traits, Tag>;
} // namespace clu
