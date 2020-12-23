#pragma once

#include <tuple>
#include <limits>
#include <typeinfo>

#include "new.h"
#include "take.h"
#include "concepts.h"
#include "function_traits.h"
#include "type_traits.h"

namespace clu
{
    template <typename Signature, Signature MemPtr>
    struct member_sig
    {
        using signature = Signature;
        static constexpr Signature member_ptr = MemPtr;
    };

    namespace type_erasure_policy
    {
        struct policy_base {};
        template <typename T> concept policy = std::derived_from<T, policy_base>;

        // @formatter:off
        struct nullable : policy_base {};
        struct copyable : policy_base {};
        template <size_t Size> struct small_buffer : policy_base { static constexpr size_t size = Size; };
        template <size_t Size> struct stack_only : policy_base { static constexpr size_t size = Size; };
        // @formatter:on
    }

    namespace detail
    {
        namespace tep = type_erasure_policy;

        template <typename Impl, template <typename> typename Members, typename Sfinae = void>
        struct has_members : std::false_type {};
        template <typename Impl, template <typename> typename Members>
        struct has_members<Impl, Members, std::void_t<Members<Impl>>> : std::true_type {};

        template <typename Impl, template <typename> typename Members>
        inline constexpr bool has_members_v = has_members<Impl, Members>::value;

        struct omnipotype
        {
            template <typename T>
            explicit(false) operator T() const { return std::declval<T>(); }
        };

        template <typename Model>
        using prototype = typename Model::template interface<meta::empty_type_list>;

        using copy_ctor_t = void* (*)(void* buf, const void* from);
        using move_ctor_t = void (*)(void* buf, void* from) noexcept;
        using dtor_t = void (*)(void*) noexcept;

        // @formatter:off
        template <bool Copyable> struct copy_vfptr_provider {};
        template <> struct copy_vfptr_provider<true> { copy_ctor_t copy_ctor = nullptr; };

        template <typename T, size_t BufferSize>
        concept stack_storable =
            sizeof(T) <= BufferSize &&
            alignof(T) <= alignof(std::max_align_t) &&
            std::is_nothrow_move_constructible_v<T>;
        // @formatter:on

        template <typename Model, bool Copyable>
        struct vtable : copy_vfptr_provider<Copyable>
        {
        private:
            template <typename T, size_t BufferSize>
            static void* copy_impl(void* buf, const void* from)
            {
                void* result;
                if constexpr (stack_storable<T, BufferSize>)
                    result = buf;
                else
                    result = aligned_alloc_for<T>();
                new(result) T(*static_cast<const T*>(from));
                return result;
            }

            template <typename T>
            static void move_impl(void* buf, void* from) noexcept { new(buf) T(static_cast<T&&>(*static_cast<T*>(from))); }

            template <typename T, size_t BufferSize>
            static void dtor_impl(void* ptr) noexcept
            {
                static_cast<T*>(ptr)->~T();
                if constexpr (!stack_storable<T, BufferSize>) aligned_free_for<T>(ptr);
            }

            template <typename T, typename Sig>
            static constexpr auto gen_one_vfptr()
            {
                using traits = function_traits<typename Sig::signature>;
                using added_const = std::conditional_t<traits::is_const,
                    const typename traits::class_type, typename traits::class_type>;
                using added_cv = std::conditional_t<traits::is_volatile,
                    volatile added_const, added_const>;
                using added_cvref = std::conditional_t<traits::is_rvalue_ref,
                    added_cv&&, added_cv&>;

                return []<typename... As>(meta::type_list<As...>) -> typename traits::return_type(*)(void*, As...)
                {
                    using return_type = typename traits::return_type;
                    return [](void* ptr, As ... args) -> return_type
                    {
                        auto&& ref = static_cast<added_cvref>(
                            *static_cast<typename traits::class_type*>(ptr));
                        return static_cast<return_type>(std::invoke(Sig::member_ptr,
                            std::forward<added_cvref>(ref), static_cast<As>(args)...));
                    };
                }(typename traits::argument_types{});
            }

            template <typename T>
            static constexpr auto gen_vfptrs()
            {
                return []<typename... Ts>(meta::type_list<Ts...>)
                {
                    return std::tuple{ gen_one_vfptr<T, Ts>()... };
                }(typename Model::template members<T>{});
            }

            using vfptrs_t = decltype(gen_vfptrs<prototype<Model>>());

        public:
            move_ctor_t move_ctor = nullptr;
            dtor_t dtor = nullptr;
            vfptrs_t vfptrs;

            template <typename T, size_t BufferSize>
            static constexpr vtable generate_for()
            {
                vtable result;
                if constexpr (Copyable) result.copy_ctor = copy_impl<T, BufferSize>;
                if constexpr (stack_storable<T, BufferSize>) result.move_ctor = move_impl<T>;
                result.dtor = dtor_impl<T, BufferSize>;
                result.vfptrs = gen_vfptrs<T>();
                return result;
            }
        };

        template <typename Vtable, typename T, size_t BufferSize>
        inline constexpr Vtable vtable_for = Vtable::template generate_for<T, BufferSize>();

        class heap_storage
        {
        private:
            void* ptr_ = nullptr;

        public:
            static constexpr size_t buffer_size = 0;

            heap_storage() = default;

            template <typename T, typename... Ts>
            void emplace(Ts&&... args)
            {
                ptr_ = aligned_alloc_for<T>();
                try { new(ptr_) T(std::forward<Ts>(args)...); }
                catch (...)
                {
                    aligned_free_for<T>(ptr_);
                    throw;
                }
            }

            void copy(const heap_storage& other, const copy_ctor_t ctor) { ptr_ = ctor(nullptr, other.ptr_); }
            void move(heap_storage&& other, move_ctor_t) noexcept { ptr_ = take(other.ptr_); }
            void* ptr() const noexcept { return ptr_; }
        };

        template <size_t N>
        class small_buffer_storage // NOLINT(cppcoreguidelines-pro-type-member-init)
        {
        private:
            void* ptr_ = nullptr;
            alignas(std::max_align_t) char buffer_[N];

        public:
            static constexpr size_t buffer_size = N;

            small_buffer_storage() = default; // NOLINT(cppcoreguidelines-pro-type-member-init)

            template <typename T, typename... Ts>
            void emplace(Ts&&... args)
            {
                if constexpr (!stack_storable<T, N>)
                {
                    ptr_ = aligned_alloc_for<T>();
                    try { new(ptr_) T(std::forward<Ts>(args)...); }
                    catch (...)
                    {
                        aligned_free_for<T>(ptr_);
                        throw;
                    }
                }
                else
                {
                    new(buffer_) T(std::forward<Ts>(args)...);
                    ptr_ = buffer_;
                }
            }

            void copy(const small_buffer_storage& other, const copy_ctor_t ctor) { ptr_ = ctor(buffer_, other.ptr_); }

            void move(small_buffer_storage&& other, const move_ctor_t ctor) noexcept
            {
                if (ctor)
                {
                    ctor(buffer_, other.ptr_);
                    ptr_ = buffer_;
                }
                else
                    ptr_ = take(other.ptr_);
            }

            void* ptr() const noexcept { return ptr_; }
        };

        template <size_t N>
        class stack_only_storage // NOLINT(cppcoreguidelines-pro-type-member-init)
        {
        private:
            alignas(std::max_align_t) char buffer_[N];

        public:
            static constexpr size_t buffer_size = N;

            stack_only_storage() = default;

            template <typename T, typename... Ts>
            void emplace(Ts&&... args) { new(buffer_) T(std::forward<Ts>(args)...); }

            void copy(const stack_only_storage& other, const copy_ctor_t ctor) { (void)ctor(buffer_, other.buffer_); }
            void move(stack_only_storage&& other, const move_ctor_t ctor) noexcept { ctor(buffer_, other.buffer_); }

            void* ptr() noexcept { return buffer_; }
            const void* ptr() const noexcept { return buffer_; }
        };

        // @formatter:off
        template <typename, template <size_t> typename>
        struct sized_template : std::false_type { static constexpr size_t size = 0; };
        template <size_t S, template <size_t> typename T>
        struct sized_template<T<S>, T> : std::true_type { static constexpr size_t size = S; };
        // @formatter:on

        template <template <size_t> typename Templ, tep::policy... Ps>
        constexpr size_t get_policy_size() noexcept { return (sized_template<Ps, Templ>::size + ... + 0); }

        template <tep::policy... Ps>
        constexpr auto get_storage_type() noexcept
        {
            constexpr size_t small_buffer_count =
                (static_cast<size_t>(sized_template<Ps, tep::small_buffer>::value) + ... + 0);
            constexpr size_t stack_only_count =
                (static_cast<size_t>(sized_template<Ps, tep::stack_only>::value) + ... + 0);
            if constexpr (small_buffer_count + stack_only_count == 0)
                return meta::type_tag<heap_storage>{};
            else
            {
                static_assert(small_buffer_count + stack_only_count == 1,
                    "At most one of small_buffer or stack_only policy can be specified");
                if constexpr (small_buffer_count > 0)
                    return meta::type_tag<small_buffer_storage<get_policy_size<tep::small_buffer, Ps...>()>>{};
                else // small_buffer_count > 0
                    return meta::type_tag<stack_only_storage<get_policy_size<tep::stack_only, Ps...>()>>{};
            }
        }

        template <tep::policy... Ps>
        using storage_type = typename decltype(get_storage_type<Ps...>())::type;

        template <typename Model, tep::policy... Policies>
        class dispatch_base
        {
        protected:
            static constexpr bool copyable =
                (std::is_same_v<Policies, tep::copyable> || ...);
            using storage_t = storage_type<Policies...>;
            static constexpr size_t buffer_size = storage_t::buffer_size;
            using vtable_t = vtable<Model, copyable>;

            void reset() noexcept
            {
                if (vtable_) vtable_->dtor(storage_.ptr());
                vtable_ = nullptr;
            }

            storage_t storage_;
            const vtable_t* vtable_ = nullptr;

            template <typename T, typename... Ts>
            void emplace_no_reset(Ts&&... args)
            {
                storage_.template emplace<T>(std::forward<Ts>(args)...);
                vtable_ = &vtable_for<vtable_t, T, buffer_size>;
            }

        public:
            dispatch_base() noexcept = default; // NOLINT(cppcoreguidelines-pro-type-member-init)

            dispatch_base(const dispatch_base&) = delete;
            dispatch_base& operator=(const dispatch_base&) = delete;

            dispatch_base(const dispatch_base& other) requires copyable
            {
                if (!other.vtable_) return;
                storage_.copy(other.storage_, other.vtable_->copy_ctor);
                vtable_ = other.vtable_;
            }

            dispatch_base& operator=(const dispatch_base& other) requires copyable
            {
                if (&other != this)
                {
                    reset();
                    if (!other.vtable_) return *this;
                    storage_.copy(other.storage_, other.vtable_->copy_ctor);
                    vtable_ = other.vtable_;
                }
                return *this;
            }

            ~dispatch_base() noexcept { reset(); }

            dispatch_base(dispatch_base&& other) noexcept
            {
                if (!other.vtable_) return;
                storage_.move(std::move(other.storage_), other.vtable_->move_ctor);
                vtable_ = take(other.vtable_);
            }

            dispatch_base& operator=(dispatch_base&& other) noexcept
            {
                if (&other != this)
                {
                    reset();
                    if (!other.vtable_) return *this;
                    storage_.move(std::move(other.storage_), other.vtable_->move_ctor);
                    vtable_ = take(other.vtable_);
                }
                return *this;
            }

            template <size_t I, typename This, typename... Ts>
            friend decltype(auto) vdispatch_impl(This&& this_, Ts&&... args); // NOLINT
        };

        template <size_t I, typename This, typename... Ts>
        decltype(auto) vdispatch_impl(This&& this_, Ts&&... args)
        {
            return get<I>(this_.vtable_->vfptrs)(
                this_.storage_.ptr(), std::forward<Ts>(args)...);
        }
    }

    template <size_t I, typename Interface, typename... Ts>
    decltype(auto) vdispatch(Interface&& inter, Ts&&... args)
    {
        if constexpr (!std::is_base_of_v<meta::empty_type_list, std::remove_cvref_t<Interface>>)
            return detail::vdispatch_impl<I>(
                std::forward<Interface>(inter), std::forward<Ts>(args)...);
        else
            return detail::omnipotype{};
    }

    template <typename Model, type_erasure_policy::policy... Policies>
    class type_erased : public Model::template interface<detail::dispatch_base<Model, Policies...>>
    {
        static_assert(detail::has_members_v<detail::prototype<Model>, Model::template members>,
            "Interface of the model type should implement the specified members");

    private:
        using indirect_base = detail::dispatch_base<Model, Policies...>;
        using base = typename Model::template interface<indirect_base>;
        using vtable_t = typename indirect_base::vtable_t;

        static constexpr bool nullable =
            (std::is_same_v<Policies, type_erasure_policy::nullable> || ...);
        static constexpr bool copyable = indirect_base::copyable;
        static constexpr bool stack_only = detail::sized_template<
            typename indirect_base::storage_t, detail::stack_only_storage>::value;

        template <typename T>
        static constexpr bool is_impl_type =
            detail::has_members_v<T, Model::template members> &&
            (!copyable || std::is_copy_constructible_v<T>) &&
            !forwarding<T, type_erased> &&
            (!stack_only || detail::stack_storable<T, indirect_base::buffer_size>);

        template <typename T>
        static constexpr const auto* vtptr_for() { return &detail::vtable_for<vtable_t, T, indirect_base::buffer_size>; }
        template <typename T>
        bool compare_vtable() const noexcept { return indirect_base::vtable_ == vtptr_for<T>(); }

    public:
        type_erased() = delete;
        type_erased() noexcept requires nullable = default;

        // @formatter:off
        template <typename T, typename... Ts> requires (is_impl_type<T> && no_cvref_v<T>)
        explicit type_erased(std::in_place_type_t<T>, Ts&&... args)
        {
            indirect_base::template emplace_no_reset<T>(
                std::forward<Ts>(args)...);
        }
        // @formatter:on

        template <typename T> requires is_impl_type<std::remove_cvref_t<T>>
        explicit(false) type_erased(T&& value)
        {
            indirect_base::template
                emplace_no_reset<std::remove_cvref_t<T>>(std::forward<T>(value));
        }

        void reset() noexcept requires nullable { indirect_base::reset(); }

        template <typename T, typename... Ts> requires (is_impl_type<T> && no_cvref_v<T>)
        void emplace(Ts&&... args)
        {
            indirect_base::reset();
            indirect_base::template emplace_no_reset<T>(std::forward<Ts>(args)...);
        }

        template <typename T>
        [[nodiscard]] T& as()
        {
            if (auto* ptr = as_ptr<T>()) return *ptr;
            throw std::bad_cast();
        }

        template <typename T>
        [[nodiscard]] const T& as() const
        {
            if (const auto* ptr = as_ptr<T>()) return *ptr;
            throw std::bad_cast();
        }

        template <typename T> requires (is_impl_type<T> && no_cvref_v<T>)
        [[nodiscard]] T* as_ptr() noexcept
        {
            return compare_vtable<T>()
                       ? static_cast<T*>(indirect_base::storage_.ptr())
                       : nullptr;
        }

        template <typename T> requires (is_impl_type<T> && no_cvref_v<T>)
        [[nodiscard]] const T* as_ptr() const noexcept
        {
            return compare_vtable<T>()
                       ? static_cast<const T*>(indirect_base::storage_.ptr())
                       : nullptr;
        }

        [[nodiscard]] bool empty() const noexcept { return indirect_base::vtable_ != nullptr; }
        [[nodiscard]] explicit operator bool() const noexcept { return indirect_base::vtable_ != nullptr; }
        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return indirect_base::vtable_ == nullptr; }
    };
}
