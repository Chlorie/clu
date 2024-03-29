#pragma once

#include <memory_resource>
#include <algorithm>

#include "scope.h"
#include "macros.h"
#include "concepts.h"
#include "assertion.h"
#include "memory.h"

namespace clu
{
    namespace detail
    {
        template <typename T>
        concept polymorphically_destructed = std::has_virtual_destructor_v<T> && (!std::is_final_v<T>);

        template <typename T, typename Alloc, typename Self>
        class box_base
        {
        public:
            using allocator_type = Alloc;

        protected:
            using alloc_traits = std::allocator_traits<Alloc>;
            static constexpr bool pocma = alloc_traits::propagate_on_container_move_assignment::value;
            static constexpr bool pocs = alloc_traits::propagate_on_container_swap::value;

        public:
            CLU_NON_COPYABLE_TYPE(box_base);
            box_base() noexcept = default;
            explicit box_base(const Alloc& alloc) noexcept: alloc_(alloc) {}
            ~box_base() noexcept { self().reset(); }
            box_base(box_base&&) = default;
            box_base& operator=(box_base&&) = default;

            [[nodiscard]] allocator_type get_allocator() const noexcept { return alloc_; }

            [[nodiscard]] T* get() noexcept { return ptr_; }
            [[nodiscard]] const T* get() const noexcept { return ptr_; }

            [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }
            [[nodiscard]] bool empty() const noexcept { return !ptr_; }

            void swap(Self& other) noexcept
            {
                if constexpr (!pocs)
                    CLU_ASSERT(alloc_ == other.alloc_, //
                        "Swapping two boxes with different non-POCS allocators is not allowed");
                else // pocs, Alloc should be Swappable
                    std::swap(alloc_, other.alloc_);
                self().swap_content(other);
            }

        protected:
            T* ptr_ = nullptr;
            CLU_NO_UNIQUE_ADDRESS Alloc alloc_;

            void move_ct_with_alloc(Self&& other) noexcept(alloc_traits::is_always_equal::value)
            {
                if constexpr (alloc_traits::is_always_equal::value)
                    self().move_same_alloc(std::move(other.self())); // NOLINT(bugprone-branch-clone)
                else if (alloc_ == other.alloc_)
                    self().move_same_alloc(std::move(other.self()));
                else
                    self().move_different_alloc(std::move(other.self()));
            }

            void move_asgn(Self&& other) noexcept(pocma)
            {
                self().reset();
                if constexpr (pocma)
                {
                    alloc_ = std::move(other.alloc_);
                    self().move_same_alloc(std::move(other.self()));
                }
                else
                {
                    if (alloc_ == other.alloc_)
                        self().move_same_alloc(std::move(other.self()));
                    else
                        self().move_different_alloc(std::move(other.self()));
                }
            }

        private:
            Self& self() noexcept { return *static_cast<Self*>(this); }
            void swap_content(Self& other) noexcept { std::swap(ptr_, other.ptr_); }
        };

        template <typename T, typename Alloc, typename Self, bool poly = polymorphically_destructed<T>>
        class box_single_base : public box_base<T, Alloc, Self>
        {
        private:
            using base = box_base<T, Alloc, Self>;
            friend base;
            template <typename, typename, typename, bool>
            friend class box_single_base;

        public:
            using base::base;

            [[nodiscard]] T& operator*() noexcept { return *this->ptr_; }
            [[nodiscard]] const T& operator*() const noexcept { return *this->ptr_; }
            [[nodiscard]] T* operator->() noexcept { return this->ptr_; }
            [[nodiscard]] const T* operator->() const noexcept { return this->ptr_; }

            void reset() noexcept
            {
                if (!this->ptr_)
                    return;
                clu::delete_object_using_allocator(this->alloc_, this->ptr_);
                this->ptr_ = nullptr;
            }

        protected:
            void set_ptr(T* ptr) noexcept { this->ptr_ = ptr; }

            void move_same_alloc(Self&& other) noexcept { this->ptr_ = std::exchange(other.ptr_, nullptr); }

            void move_different_alloc(Self&& other)
            {
                this->ptr_ = clu::new_object_using_allocator<T>(this->alloc_, std::move(*other.ptr_));
            }
        };

        template <typename T, typename Alloc>
        inline constexpr auto box_deleter_of = +[](void* p, const rebound_allocator_t<Alloc, std::byte>& alloc) noexcept
        {
            using rebound = rebound_allocator_t<Alloc, T>;
            using traits = std::allocator_traits<rebound>;
            rebound al(alloc);
            traits::destroy(al, static_cast<T*>(p));
            traits::deallocate(al, static_cast<T*>(p), 1);
        };

        template <typename T, typename Alloc, typename Self>
        class box_single_base<T, Alloc, Self, true> : public box_single_base<T, Alloc, Self, false>
        {
        private:
            using base = box_single_base<T, Alloc, Self, false>;
            friend box_base<T, Alloc, Self>;
            template <typename, typename, typename, bool>
            friend class box_single_base;

        public:
            using base::base;

            // clang-format off
            template <std::derived_from<T> U>
            [[nodiscard]] U* get_if() noexcept { return dynamic_cast<U*>(this->ptr_); }

            template <std::derived_from<T> U>
            [[nodiscard]] const U* get_if() const noexcept { return dynamic_cast<const U*>(this->ptr_); }

            template <std::derived_from<T> U>
            [[nodiscard]] U& get_ref() noexcept { return dynamic_cast<U&>(**this); }

            template <std::derived_from<T> U>
            [[nodiscard]] const U& get_ref() const noexcept { return dynamic_cast<const U&>(**this); }
            // clang-format on

            void reset() noexcept
            {
                if (!this->ptr_)
                    return;
                dtor_(dynamic_cast<void*>(this->ptr_), this->alloc_);
                this->ptr_ = nullptr;
            }

        protected:
            void (*dtor_)(void* ptr, const rebound_allocator_t<Alloc, std::byte>& alloc) noexcept = nullptr;

            void set_ptr(T* ptr) noexcept
            {
                this->ptr_ = ptr;
                dtor_ = box_deleter_of<T, Alloc>;
            }

            void swap_content(Self& other) noexcept
            {
                std::swap(this->ptr_, other.ptr_);
                std::swap(dtor_, other.dtor_);
            }

            void move_same_alloc(Self&& other) noexcept
            {
                this->ptr_ = std::exchange(other.ptr_, nullptr);
                this->dtor_ = other.dtor_;
            }

            template <typename D, typename DAlloc, typename DSelf>
            void move_from_derived(box_single_base<D, DAlloc, DSelf>& other) noexcept
            {
                this->ptr_ = std::exchange(other.ptr_, nullptr);
                if constexpr (polymorphically_destructed<D>)
                    this->dtor_ = other.dtor_;
                else
                    this->dtor_ = box_deleter_of<D, Alloc>;
            }
        };

        template <typename D, typename T>
        concept box_emplacable = std::same_as<D, T> || (polymorphically_destructed<T> && std::derived_from<D, T>);
    } // namespace detail

    inline constexpr struct nullbox_t
    {
    } nullbox{};

    template <typename T, typename Alloc = std::allocator<std::remove_extent_t<T>>>
    class box : public detail::box_single_base<T, Alloc, box<T, Alloc>>
    {
    private:
        using base = detail::box_single_base<T, Alloc, box>;

    public:
        using value_type = T;
        static_assert(allocator_for<Alloc, T>, //
            "The value type of the allocator does not match with the box's value type");
        static_assert(std::is_same_v<typename std::allocator_traits<Alloc>::pointer, T*>,
            "The allocator must not allocate a fancy pointer");

        explicit(false) box(nullbox_t) noexcept {}

        box(std::allocator_arg_t, const Alloc& alloc, nullbox_t) noexcept: base(alloc) {}

        box(box&& other) noexcept: base(other.alloc_) { this->move_same_alloc(std::move(other)); }

        box(std::allocator_arg_t, const Alloc& alloc, box&& other) //
            noexcept(base::alloc_traits::is_always_equal::value)
            requires base::alloc_traits::is_always_equal::value ||
            (std::move_constructible<T> && !detail::polymorphically_destructed<T>)
            : base(alloc)
        {
            this->move_ct_with_alloc(std::move(other));
        }

        template <typename... Args>
            requires std::constructible_from<T, Args...> && constructible_using_allocator_from<T, Alloc, Args...>
        explicit box(Args&&... args): box(std::allocator_arg, Alloc{}, static_cast<Args&&>(args)...)
        {
        }

        template <typename... Args>
            requires std::constructible_from<T, Args...> && constructible_using_allocator_from<T, Alloc, Args...>
        box(std::allocator_arg_t, const Alloc& alloc, Args&&... args): detail::box_single_base<T, Alloc, box>(alloc)
        {
            this->set_ptr(clu::new_object_using_allocator<T>(alloc, static_cast<Args&&>(args)...));
        }

        template <typename D>
            requires detail::polymorphically_destructed<T> && proper_subclass_of<D, T>
        explicit(false) box(box<D, rebound_allocator_t<Alloc, D>>&& other) noexcept:
            detail::box_single_base<T, Alloc, box>(other.get_allocator())
        {
            this->move_from_derived(other);
        }

        box& operator=(box&& other) noexcept(base::pocma)
            requires base::pocma || (std::move_constructible<T> && !detail::polymorphically_destructed<T>)
        {
            if (&other != this)
                this->move_asgn(std::move(other));
            return *this;
        }

        template <typename D>
            requires detail::polymorphically_destructed<T> && proper_subclass_of<D, T> && base::pocma
        box& operator=(box<D, rebound_allocator_t<Alloc, D>>&& other) noexcept
        {
            this->reset();
            this->alloc_ = other.get_allocator();
            this->move_from_derived(other);
            return *this;
        }

        box& operator=(nullbox_t) noexcept
        {
            this->reset();
            return *this;
        }

        template <detail::box_emplacable<T> D = T, typename... Args>
            requires std::constructible_from<D, Args...> && constructible_using_allocator_from<D, Alloc, Args...>
        T& emplace(Args&&... args)
        {
            if constexpr (detail::polymorphically_destructed<T>)
            {
                this->reset();
                this->dtor_ = detail::box_deleter_of<D, Alloc>;
                this->ptr_ = clu::new_object_using_allocator<D>(this->alloc_, static_cast<Args&&>(args)...);
            }
            else if (this->ptr_)
            {
                base::alloc_traits::destroy(this->alloc_, this->ptr_);
                scope_fail guard{[&] { base::alloc_traits::deallocate(this->alloc_, this->ptr_, 1); }};
                base::alloc_traits::construct(this->alloc_, this->ptr_, static_cast<Args&&>(args)...);
            }
            else
                this->ptr_ = clu::new_object_using_allocator<D>(this->alloc_, static_cast<Args&&>(args)...);
            return *this->ptr_;
        }

        std::shared_ptr<T> to_shared() &&
        {
            if constexpr (detail::polymorphically_destructed<T>)
            {
                return std::shared_ptr<T>(
                    std::exchange(this->ptr_, nullptr),
                    [al = this->alloc_, d = this->dtor_](T* ptr) noexcept { d(ptr, al); }, //
                    this->alloc_);
            }
            else
            {
                return std::shared_ptr<T>(
                    std::exchange(this->ptr_, nullptr),
                    [al = this->alloc_](T* ptr) mutable noexcept
                    {
                        base::alloc_traits::destroy(al, ptr);
                        base::alloc_traits::deallocate(al, ptr, 1);
                    },
                    this->alloc_);
            }
        }

        friend void swap(box& lhs, box& rhs) noexcept { lhs.swap(rhs); }
    };

    template <typename T, typename Alloc>
    class box<T[], Alloc> : public detail::box_base<T, Alloc, box<T[], Alloc>>
    {
    private:
        using base = detail::box_base<T, Alloc, box>;
        friend base;

    public:
        using iterator = T*;
        using const_iterator = const T*;
        using value_type = T;
        static_assert(allocator_for<Alloc, T>, //
            "The value type of the allocator does not match with the box's value type");
        static_assert(std::is_same_v<typename std::allocator_traits<Alloc>::pointer, T*>,
            "The allocator must not allocate a fancy pointer");

        explicit(false) box(nullbox_t) noexcept {}

        box(std::allocator_arg_t, const Alloc& alloc, nullbox_t) noexcept: base(alloc) {}

        explicit box(const std::size_t size): box(std::allocator_arg, Alloc{}, size) {}

        box(std::allocator_arg_t, const Alloc& alloc, const std::size_t size): base(alloc), size_(size)
        {
            this->ptr_ = base::alloc_traits::allocate(this->alloc_, size_);
            scope_fail guard{[&] { base::alloc_traits::deallocate(this->alloc_, this->ptr_, size_); }};
            clu::uninitialized_value_construct_n_using_allocator(this->ptr_, size, alloc);
        }

        box(box&& other) noexcept: base(other.alloc_) { this->move_same_alloc(std::move(other)); }

        box(std::allocator_arg_t, const Alloc& alloc, box&& other)
            requires std::move_constructible<T>
            : base(alloc)
        {
            this->move_ct_with_alloc(std::move(other));
        }

        box& operator=(box&& other) noexcept(base::pocma)
            requires base::pocma || std::move_constructible<T>
        {
            if (&other != this)
                this->move_asgn(std::move(other));
            return *this;
        }

        box& operator=(nullbox_t) noexcept
        {
            this->reset();
            return *this;
        }

        std::shared_ptr<T[]> to_shared() &&
        {
            return std::shared_ptr<T[]>(
                std::exchange(this->ptr_, nullptr),
                [al = this->alloc_, n = std::exchange(size_, 0)](T* ptr) mutable noexcept
                {
                    clu::destroy_n_using_allocator(ptr, n, al);
                    base::alloc_traits::deallocate(al, ptr, n);
                },
                this->alloc_);
        }

        friend void swap(box& lhs, box& rhs) noexcept { lhs.swap(rhs); }

        [[nodiscard]] T* begin() noexcept { return this->ptr_; }
        [[nodiscard]] const T* begin() const noexcept { return this->ptr_; }
        [[nodiscard]] const T* cbegin() const noexcept { return this->ptr_; }
        [[nodiscard]] T* end() noexcept { return this->ptr_ + size_; }
        [[nodiscard]] const T* end() const noexcept { return this->ptr_ + size_; }
        [[nodiscard]] const T* cend() const noexcept { return this->ptr_ + size_; }

        [[nodiscard]] T& operator[](const std::size_t i) noexcept { return (this->ptr_)[i]; }
        [[nodiscard]] const T& operator[](const std::size_t i) const noexcept { return (this->ptr_)[i]; }
        [[nodiscard]] std::size_t size() const noexcept { return size_; }

        void reset() noexcept
        {
            if (!this->ptr_)
                return;
            clu::destroy_n_using_allocator(this->ptr_, size_, this->alloc_);
            base::alloc_traits::deallocate(this->alloc_, this->ptr_, size_);
            this->ptr_ = nullptr;
            this->size_ = 0;
        }

    private:
        std::size_t size_ = 0;

        void move_same_alloc(box&& other) noexcept
        {
            this->ptr_ = std::exchange(other.ptr_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }

        void move_different_alloc(box&& other)
        {
            T* ptr = base::alloc_traits::allocate(this->alloc_, other.size_);
            std::size_t i = 0;
            scope_fail guard{[&]
                {
                    while (i-- > 0)
                        base::alloc_traits::destroy(this->alloc_, ptr + i);
                    base::alloc_traits::deallocate(this->alloc_, ptr, other.size_);
                }};
            for (; i < other.size_; i++)
                base::alloc_traits::construct(this->alloc_, ptr + i, std::move(other.ptr_[i]));
            this->ptr_ = ptr;
            this->size_ = other.size_;
        }
    };

    template <typename T, allocator Alloc>
        requires(!std::is_bounded_array_v<T>)
    auto allocate_box(const Alloc& alloc, nullbox_t) noexcept
    {
        using rebound = rebound_allocator_t<Alloc, std::remove_extent_t<T>>;
        return box<T, rebound>(std::allocator_arg, rebound(alloc), nullbox);
    }

    template <typename T, allocator Alloc, typename... Args>
        requires(!std::is_array_v<T>) && //
        std::constructible_from<T, Args...> && //
        constructible_using_allocator_from<T, Alloc, Args...>
    auto allocate_box(const Alloc& alloc, Args&&... args)
    {
        using rebound = rebound_allocator_t<Alloc, T>;
        return box<T, rebound>(std::allocator_arg, rebound(alloc), static_cast<Args&&>(args)...);
    }

    template <typename T, allocator Alloc>
        requires std::is_unbounded_array_v<T>
    auto allocate_box(const Alloc& alloc, const std::size_t size)
    {
        using rebound = rebound_allocator_t<Alloc, std::remove_extent_t<T>>;
        return box<T, rebound>(std::allocator_arg, rebound(alloc), size);
    }

    template <typename T, allocator Alloc>
        requires std::is_bounded_array_v<T>
    void allocate_box(Alloc, auto&&...) = delete;

    namespace pmr
    {
        template <typename T>
        using box = box<T, std::pmr::polymorphic_allocator<std::remove_extent_t<T>>>;
    }
} // namespace clu
