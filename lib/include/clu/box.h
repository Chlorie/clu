#pragma once

#include <memory_resource>
#include <algorithm>

#include "scope.h"
#include "macros.h"
#include "concepts.h"

namespace clu
{
    namespace detail
    {
        using pmr_alloc = std::pmr::polymorphic_allocator<>;
        using mem_res = std::pmr::memory_resource;
        using deleter_type = void (*)(pmr_alloc alloc, void* ptr) noexcept;

        template <typename T>
        constexpr deleter_type deleter_of = [](pmr_alloc a, void* p) noexcept { a.delete_object(static_cast<T*>(p)); };

        template <typename T, typename Self>
        class box_base
        {
        public:
            using allocator_type = pmr_alloc;

            CLU_NON_COPYABLE_TYPE(box_base);
            box_base() noexcept = default;
            explicit box_base(mem_res* mem) noexcept: mem_(mem) {}
            ~box_base() noexcept { self().reset(); }
            box_base(box_base&& other) noexcept: ptr_(std::exchange(other.ptr_, nullptr)), mem_(other.mem_) {}

            box_base& operator=(box_base&& other) noexcept
            {
                if (&other != this)
                {
                    self().reset();
                    ptr_ = std::exchange(other.ptr_, nullptr);
                    mem_ = other.mem_;
                }
                return *this;
            }

            [[nodiscard]] allocator_type get_allocator() const noexcept { return mem_; }

            [[nodiscard]] T* get() noexcept { return ptr_; }
            [[nodiscard]] const T* get() const noexcept { return ptr_; }

            [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }
            [[nodiscard]] bool empty() const noexcept { return !ptr_; }

        protected:
            T* ptr_ = nullptr;
            mem_res* mem_ = nullptr;

            void swap(box_base& other) noexcept
            {
                std::swap(ptr_, other.ptr_);
                std::swap(mem_, other.mem_);
            }

        private:
            Self& self() noexcept { return *static_cast<Self*>(this); }
        };

        template <typename T>
        concept polymorphically_destroyed = std::has_virtual_destructor_v<T> && (!std::is_final_v<T>);

        template <typename T, typename Self>
        class box_single_base : public box_base<T, Self>
        {
        public:
            using box_base<T, Self>::box_base;

            [[nodiscard]] T& operator*() noexcept { return *this->ptr_; }
            [[nodiscard]] const T& operator*() const noexcept { return *this->ptr_; }
            [[nodiscard]] T* operator->() noexcept { return this->ptr_; }
            [[nodiscard]] const T* operator->() const noexcept { return this->ptr_; }

        protected:
            void swap(box_single_base& other) noexcept { box_base<T, Self>::swap(other); }

            void set_ptr(T* ptr) noexcept { this->ptr_ = ptr; }
        };

        template <typename T, typename D>
        class box_cast_base : public box_single_base<T, D>
        {
        public:
            using box_single_base<T, D>::box_single_base;

            void reset() noexcept
            {
                if (this->ptr_)
                    this->get_allocator().delete_object(this->ptr_);
            }

            template <typename, typename>
            friend class box_cast_base;
        };

        template <polymorphically_destroyed T, typename Self>
        class box_cast_base<T, Self> : public box_single_base<T, Self>
        {
        public:
            using box_single_base<T, Self>::box_single_base;

            box_cast_base(box_cast_base&& other) noexcept:
                box_single_base(std::move(other)), //
                allocated_(std::exchange(other.allocated_, nullptr)), //
                deleter_(other.deleter_)
            {
            }

            box_cast_base& operator=(box_cast_base&& other) noexcept
            {
                if (&other != this)
                {
                    box_single_base<T, Self>::operator=(std::move(other));
                    allocated_ = std::exchange(other.allocated_, nullptr);
                    deleter_ = other.deleter_;
                }
                return *this;
            }

            void reset() noexcept
            {
                if (!this->allocated_)
                    return;
                deleter_(this->get_allocator(), this->allocated_);
                this->allocated_ = nullptr;
                this->ptr_ = nullptr;
            }

        protected:
            // allocated_ points to the address allocated originally (say as a U*)
            // It might be different to static_cast<void*>(ptr_), depending on the offset between T* and U*
            void* allocated_ = nullptr;
            deleter_type deleter_ = nullptr;

            void swap(box_cast_base& other) noexcept
            {
                box_base<T, Self>::swap(other);
                std::swap(allocated_, other.allocated_);
                std::swap(deleter_, other.deleter_);
            }

            void set_ptr(T* ptr) noexcept
            {
                this->ptr_ = ptr;
                this->allocated_ = ptr;
                deleter_ = deleter_of<T>;
            }

            template <typename D, typename DSelf>
            void move_from_derived(box_cast_base<D, DSelf>& other) noexcept
            {
                if constexpr (detail::polymorphically_destroyed<D>)
                {
                    this->ptr_ = std::exchange(other.ptr_, nullptr);
                    this->allocated_ = std::exchange(other.allocated_, nullptr);
                    this->mem_ = other.mem_;
                    this->deleter_ = other.deleter_;
                }
                else
                {
                    this->allocated_ = other.ptr_; // D* -> void*
                    this->ptr_ = std::exchange(other.ptr_, nullptr); // D* -> T*
                    this->mem_ = other.mem_;
                    this->deleter_ = detail::deleter_of<D>;
                }
            }

            template <typename, typename>
            friend class box_cast_base;
        };
    } // namespace detail

    template <typename T>
    class box : public detail::box_cast_base<T, box<T>>
    {
    public:
        box() noexcept = default;
        box(box&&) noexcept = default;
        box& operator=(box&&) noexcept = default;

        template <typename... Args>
            requires std::constructible_from<T, Args...>
        explicit box(Args&&... args):
            box(std::allocator_arg, std::pmr::get_default_resource(), static_cast<Args&&>(args)...)
        {
        }

        template <typename... Args>
            requires std::constructible_from<T, Args...>
        explicit box(std::allocator_arg_t, std::pmr::polymorphic_allocator<> alloc, Args&&... args):
            detail::box_cast_base<T, box>(alloc.resource())
        {
            this->set_ptr(alloc.new_object<T>(static_cast<Args&&>(args)...));
        }

        template <typename D>
            requires detail::polymorphically_destroyed<T> && proper_subclass_of<D, T>
        explicit(false) box(box<D>&& other) noexcept
        {
            this->move_from_derived(other);
        }

        template <typename D>
            requires detail::polymorphically_destroyed<T> && proper_subclass_of<D, T>
        box& operator=(box<D>&& other) noexcept
        {
            this->reset();
            this->move_from_derived(other);
            return *this;
        }

        void swap(box& other) noexcept { detail::box_cast_base<T, box>::swap(other); }
        friend void swap(box& lhs, box& rhs) noexcept { lhs.swap(rhs); }
    };

    template <typename T>
    class box<T[]> : public detail::box_base<T, box<T[]>>
    {
    public:
        box() noexcept = default;

        explicit box(const std::size_t size): box(std::allocator_arg, std::pmr::get_default_resource(), size) {}

        explicit box(std::allocator_arg_t, std::pmr::polymorphic_allocator<> alloc, const std::size_t size):
            detail::box_base<T, box>(alloc.resource()), size_(size)
        {
            this->ptr_ = alloc.allocate_object<T>(size);
            scope_fail guard{[&] { alloc.deallocate_object(this->ptr_, this->size_); }};
            std::uninitialized_value_construct_n(this->ptr_, size);
        }

        [[nodiscard]] T& operator[](const std::size_t i) noexcept { return (this->ptr_)[i]; }
        [[nodiscard]] const T& operator[](const std::size_t i) const noexcept { return (this->ptr_)[i]; }
        [[nodiscard]] std::size_t size() const noexcept { return size_; }

        void swap(box& other) noexcept { detail::box_base<T, box>::swap(other); }
        friend void swap(box& lhs, box& rhs) noexcept { lhs.swap(rhs); }

        void reset() noexcept
        {
            std::destroy_n(this->ptr_, size());
            this->get_allocator().deallocate_object(this->ptr_, this->size_);
        }

    private:
        std::size_t size_ = 0;
    };

    // ReSharper disable CppPassValueParameterByConstReference
    template <typename T, typename... Args>
        requires(!std::is_array_v<T>) && std::constructible_from<T, Args...>
    box<T> allocate_box(const std::pmr::polymorphic_allocator<> alloc, Args&&... args)
    {
        return box<T>(std::allocator_arg, alloc, static_cast<Args&&>(args)...);
    }

    template <typename T>
        requires std::is_unbounded_array_v<T>
    box<T> allocate_box(const std::pmr::polymorphic_allocator<> alloc, const std::size_t size)
    {
        return box<T>(std::allocator_arg, alloc, size);
    }

    template <typename T>
        requires std::is_bounded_array_v<T>
    void allocate_box(std::pmr::polymorphic_allocator<>, auto&&...) = delete;
    // ReSharper restore CppPassValueParameterByConstReference
} // namespace clu
