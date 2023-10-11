#pragma once

#include <functional>

#include "concepts.h"
#include "function_traits.h"
#include "meta_algorithm.h"
#include "memory.h"
#include "macros.h"

namespace clu
{
    struct copyable_function_policy
    {
        static constexpr bool copyable = true;
        static constexpr std::size_t small_buffer_size = 2 * sizeof(void*);
    };

    struct move_only_function_policy
    {
        static constexpr bool copyable = false;
        static constexpr std::size_t small_buffer_size = 2 * sizeof(void*);
    };

    namespace detail
    {
        template <typename Alloc>
        class dtor_vf
        {
        public:
            template <typename T, bool free>
            constexpr static dtor_vf get_vf() noexcept
            {
                dtor_vf vf{};
                vf.dtor_ = [](Alloc a, void* p) noexcept
                {
                    using rebound = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
                    using traits = std::allocator_traits<rebound>;
                    rebound ta(a);
                    T* tp = static_cast<T*>(p);
                    traits::destroy(ta, tp);
                    if constexpr (free)
                        traits::deallocate(ta, tp, 1);
                };
                return vf;
            }

            void dtor(Alloc a, void* p) const noexcept { dtor_(a, p); }

        private:
            void (*dtor_)(Alloc a, void* p) noexcept = nullptr;
        };

        template <typename Alloc>
        class move_vf
        {
        public:
            template <typename T, bool free>
            constexpr static move_vf get_vf() noexcept
            {
                move_vf vf{};
                vf.move_diff_ = [](Alloc newa, [[maybe_unused]] void* buf, Alloc olda, void* p) -> void*
                {
                    if constexpr (free)
                        return move_vf::move_diff_free<T>(newa, olda, p);
                    else
                        return move_vf::move_diff_no_free<T>(newa, buf, olda, p);
                };
                vf.move_same_ = []([[maybe_unused]] Alloc a, [[maybe_unused]] void* buf, void* p) noexcept -> void*
                {
                    if constexpr (free)
                        return p;
                    else
                        return move_vf::move_diff_no_free<T>(a, buf, a, p);
                };
                return vf;
            }

            void* move_same(Alloc a, void* buf, void* p) const noexcept { return move_same_(a, buf, p); }
            void* move_diff(Alloc newa, void* buf, Alloc olda, void* p) const { return move_diff_(newa, buf, olda, p); }

        private:
            void* (*move_same_)(Alloc a, void* buf, void* p) noexcept = nullptr;
            void* (*move_diff_)(Alloc newa, void* buf, Alloc olda, void* p) = nullptr;

            template <typename T>
            static void* move_diff_free(Alloc newa, Alloc olda, void* p)
            {
                T* tp = static_cast<T*>(p);
                T* newp = clu::new_object_using_allocator<T>(newa, std::move(*tp));
                clu::delete_object_using_allocator(olda, tp);
                return newp;
            }

            template <typename T>
            static void* move_diff_no_free(Alloc newa, void* buf, Alloc olda, void* p)
            {
                using rebound = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
                using traits = std::allocator_traits<rebound>;
                rebound newta(newa), oldta(olda);
                T* tp = static_cast<T*>(p);
                traits::construct(newta, static_cast<T*>(buf), std::move(*tp));
                traits::destroy(oldta, tp);
                return buf;
            }
        };

        template <typename Alloc>
        class copy_vf
        {
        public:
            template <typename T, bool free>
            constexpr static copy_vf get_vf() noexcept
            {
                copy_vf vf{};
                vf.copy_ = [](Alloc newa, [[maybe_unused]] void* buf, void* p) -> void*
                {
                    if constexpr (free)
                    {
                        T* tp = static_cast<T*>(p);
                        T* newp = clu::new_object_using_allocator<T>(newa, *tp);
                        return newp;
                    }
                    else
                    {
                        using rebound = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
                        using traits = std::allocator_traits<rebound>;
                        rebound newta(newa);
                        T* tp = static_cast<T*>(p);
                        traits::construct(newta, static_cast<T*>(buf), *tp);
                        return buf;
                    }
                };
                return vf;
            }

            void* copy(Alloc newa, void* buf, void* p) const { return copy_(newa, buf, p); }

        private:
            void* (*copy_)(Alloc newa, void* buf, void* p) = nullptr;
        };

        template <typename Alloc, bool copy = false>
        struct lifetime_vf : dtor_vf<Alloc>, move_vf<Alloc>
        {
            template <typename T, bool free>
            constexpr explicit lifetime_vf(type_tag_t<T>, std::bool_constant<free>) noexcept: //
                dtor_vf<Alloc>(dtor_vf<Alloc>::template get_vf<T, free>()), //
                move_vf<Alloc>(move_vf<Alloc>::template get_vf<T, free>())
            {
            }
        };

        template <typename Alloc>
        struct lifetime_vf<Alloc, true> : lifetime_vf<Alloc, false>, copy_vf<Alloc>
        {
            template <typename T, bool free>
            constexpr explicit lifetime_vf(type_tag_t<T>, std::bool_constant<free>) noexcept: //
                lifetime_vf<Alloc, false>(type_tag<T>, std::bool_constant<free>{}), //
                copy_vf<Alloc>(copy_vf<Alloc>::template get_vf<T, free>())
            {
            }
        };

        template <typename T, typename Alloc, bool free, bool copy>
        inline constexpr lifetime_vf<Alloc, copy> lifetime_vf_obj{type_tag<T>, std::bool_constant<free>{}};

        template <typename Ret, bool Noexcept>
        struct to_fptr_type_impl
        {
            template <typename... Ts>
            using fn = conditional_t<Noexcept, Ret (*)(void*, Ts&&...) noexcept, Ret (*)(void*, Ts&&...)>;
        };

        template <typename F>
        using to_fptr_type = meta::unpack_invoke< //
            typename function_traits<F>::argument_types, //
            to_fptr_type_impl<typename function_traits<F>::return_type, function_traits<F>::is_noexcept>>;

        template <typename T>
        concept copyable_fpol = requires { requires T::copyable; };

        // clang-format off
        template <typename T>
        concept sbo_fpol = requires { { T::small_buffer_size } -> std::convertible_to<std::size_t>; };
        // clang-format on

        template <typename>
        class sbo_storage
        {
        public:
            constexpr void* buffer() noexcept { return nullptr; }
            constexpr const void* buffer() const noexcept { return nullptr; }
        };
        template <sbo_fpol Policy>
        class sbo_storage<Policy>
        {
        public:
            // Not defaulted intentionally, since it prohibits default-initialized const objects of this
            sbo_storage() noexcept {} // NOLINT
            constexpr void* buffer() noexcept { return static_cast<void*>(&buffer_); }
            constexpr const void* buffer() const noexcept { return static_cast<const void*>(&buffer_); }

        private:
            alignas(std::max_align_t) char buffer_[Policy::small_buffer_size];
        };

        template <typename T, typename Policy>
        concept stored_in_sbo = //
            sbo_fpol<Policy> && //
            std::is_nothrow_move_constructible_v<T> && //
            (alignof(T) <= alignof(std::max_align_t)) && //
            (sizeof(T) <= Policy::small_buffer_size);

        template <typename T>
        concept alloc_nothrow_move = //
            std::allocator_traits<T>::propagate_on_container_move_assignment::value ||
            std::allocator_traits<T>::is_always_equal::value;

        template <typename T>
        concept alloc_nothrow_swap = //
            std::allocator_traits<T>::propagate_on_container_swap::value ||
            std::allocator_traits<T>::is_always_equal::value;

        template <typename Policy, typename Alloc>
        class any_sbo_object : sbo_storage<Policy>
        {
        public:
            using allocator_type = Alloc;

            any_sbo_object() noexcept = default;
            any_sbo_object(std::allocator_arg_t, const Alloc& alloc) noexcept: alloc_(alloc) {}

            template <typename T, typename... Args>
            explicit any_sbo_object(std::in_place_type_t<T>, Args&&... args):
                any_sbo_object(std::allocator_arg, Alloc{}, std::in_place_type<T>, static_cast<Args&&>(args)...)
            {
            }

            template <typename T, typename... Args>
            any_sbo_object(std::allocator_arg_t, const Alloc& alloc, std::in_place_type_t<T>, Args&&... args):
                alloc_(alloc)
            {
                if constexpr (stored_in_sbo<T, Policy>)
                {
                    using rebound = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
                    using traits = std::allocator_traits<rebound>;
                    rebound ta(alloc_);
                    traits::construct(ta, static_cast<T*>(this->buffer()), static_cast<Args&&>(args)...);
                    ptr_ = this->buffer();
                }
                else
                    ptr_ = clu::new_object_using_allocator<T>(alloc, static_cast<Args&&>(args)...);
                lifetime_vfptr_ = &lifetime_vf_obj<T, Alloc, !stored_in_sbo<T, Policy>, copyable_fpol<Policy>>;
            }

            any_sbo_object(const any_sbo_object& other)
                requires copyable_fpol<Policy>
                :
                any_sbo_object(std::allocator_arg,
                    std::allocator_traits<Alloc>::select_on_container_copy_construction(other.alloc_), other)
            {
            }

            any_sbo_object(std::allocator_arg_t, const Alloc& alloc, const any_sbo_object& other)
                requires copyable_fpol<Policy>
                : lifetime_vfptr_(other.lifetime_vfptr_), alloc_(alloc)
            {
                if (other.ptr_)
                    ptr_ = lifetime_vfptr_->copy(alloc, this->buffer(), other.ptr_);
            }

            any_sbo_object(any_sbo_object&& other) noexcept:
                lifetime_vfptr_(other.lifetime_vfptr_), alloc_(other.alloc_)
            {
                if (other.ptr_)
                {
                    ptr_ = lifetime_vfptr_->move_same(alloc_, this->buffer(), other.ptr_);
                    other.ptr_ = nullptr;
                }
            }

            any_sbo_object(std::allocator_arg_t, const Alloc& alloc, any_sbo_object&& other):
                lifetime_vfptr_(other.lifetime_vfptr_), alloc_(alloc)
            {
                if (other.ptr_)
                {
                    ptr_ = lifetime_vfptr_->move_diff(alloc, this->buffer(), other.alloc_, other.ptr_);
                    other.ptr_ = nullptr;
                }
            }

            ~any_sbo_object() noexcept
            {
                if (ptr_)
                    lifetime_vfptr_->dtor(alloc_, ptr_);
            }

            any_sbo_object& operator=(const any_sbo_object& other)
                requires copyable_fpol<Policy>
            {
                if (&other != this)
                {
                    clear();
                    lifetime_vfptr_ = other.lifetime_vfptr_;
                    if constexpr (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment)
                        alloc_ = other.alloc_;
                    ptr_ = lifetime_vfptr_->copy(alloc_, this->buffer(), other.ptr_);
                }
                return *this;
            }

            any_sbo_object& operator=(any_sbo_object&& other) noexcept(alloc_nothrow_move<Alloc>)
            {
                if (&other != this)
                {
                    clear();
                    lifetime_vfptr_ = other.lifetime_vfptr_;
                    if constexpr (alloc_nothrow_move<Alloc>)
                    {
                        alloc_ = other.alloc_;
                        ptr_ = lifetime_vfptr_->move_same(alloc_, this->buffer(), other.ptr_);
                        other.ptr_ = nullptr;
                    }
                    else if (alloc_ == other.alloc_)
                    {
                        ptr_ = lifetime_vfptr_->move_same(alloc_, this->buffer(), other.ptr_);
                        other.ptr_ = nullptr;
                    }
                    else
                    {
                        // TODO: Maybe just move the underlying data (do not reset other) in this case?
                        ptr_ = lifetime_vfptr_->move_diff(alloc_, this->buffer(), other.alloc_, other.ptr_);
                        other.ptr_ = nullptr;
                    }
                }
                return *this;
            }

            // TODO: respect POCS
            void swap(any_sbo_object& other) noexcept(alloc_nothrow_swap<Alloc>)
            {
                any_sbo_object temp(std::move(*this));
                *this = std::move(other);
                other = std::move(temp);
            }

            friend void swap(any_sbo_object& lhs, any_sbo_object& rhs) noexcept(alloc_nothrow_swap<Alloc>)
            {
                lhs.swap(rhs);
            }

            Alloc get_allocator() const noexcept { return alloc_; }
            void* get() const noexcept { return ptr_; }

            void clear() noexcept
            {
                if (ptr_)
                {
                    lifetime_vfptr_->dtor(alloc_, ptr_);
                    ptr_ = nullptr;
                }
            }

        private:
            void* ptr_ = nullptr;
            const lifetime_vf<Alloc, copyable_fpol<Policy>>* lifetime_vfptr_ = nullptr;
            CLU_NO_UNIQUE_ADDRESS Alloc alloc_;
        };

        template <typename Obj, typename Traits, typename... Ts>
        constexpr bool invocable_with_signature_impl(type_list<Ts...>) noexcept
        {
            constexpr bool nothrow = Traits::is_noexcept ? nothrow_invocable<Obj, Ts...> : true;
            return nothrow && invocable_of<Obj, typename Traits::return_type, Ts...>;
        }

        template <typename F, typename G>
        constexpr bool invocable_with_signature() noexcept
        {
            using traits = function_traits<F>;
            using copy_const = conditional_t<traits::is_const, const G, G>;
            using copy_clref = conditional_t<traits::is_lvalue_ref, copy_const&, copy_const>;
            using copy_cref = conditional_t<traits::is_lvalue_ref, copy_clref&&, copy_clref>;
            return detail::invocable_with_signature_impl<copy_cref, traits>(typename traits::argument_types{});
        }

        template <typename G, typename Traits, typename... Ts>
        constexpr auto fptr_erase_impl(type_list<Ts...>)
        {
            using copy_const = conditional_t<Traits::is_const, const G, G>;
            using copy_clref = conditional_t<Traits::is_lvalue_ref, copy_const&, copy_const>;
            using copy_cref = conditional_t<Traits::is_lvalue_ref, copy_clref&&, copy_clref>;
            return [](void* ptr, Ts&&... args) noexcept(Traits::is_noexcept) -> typename Traits::return_type
            {
                return std::invoke( //
                    static_cast<copy_cref>(*static_cast<copy_const*>(ptr)), static_cast<Ts&&>(args)...);
            };
        }

        template <typename F, typename Policy, typename Alloc, typename Self>
        class function_base
        {
        public:
            function_base() noexcept = default;
            function_base(const function_base&) = default;
            function_base(function_base&&) = default;
            function_base& operator=(const function_base&) = default;
            function_base& operator=(function_base&&) = default;
            ~function_base() noexcept = default;

            explicit function_base(const Alloc& alloc): obj_(alloc) {}

            template <typename G>
                requires(!similar_to<G, function_base>) && (detail::invocable_with_signature<F, G>())
            explicit(false) function_base(G&& func): function_base(static_cast<G&&>(func), Alloc{})
            {
            }

            template <typename G>
                requires(!similar_to<G, function_base>) && (detail::invocable_with_signature<F, G>())
            function_base(G&& func, const Alloc& alloc):
                fptr_(detail::fptr_erase_impl<std::decay_t<G>, function_traits<F>>(
                    typename function_traits<F>::argument_types{})),
                obj_(std::allocator_arg, alloc, std::in_place_type<std::decay_t<G>>, static_cast<G&&>(func))
            {
            }

            function_base(const function_base& other, const Alloc& alloc)
                requires copyable_fpol<Policy>
                : fptr_(other.fptr_), obj_(std::allocator_arg, alloc, other.obj_)
            {
            }

            function_base(function_base&& other, const Alloc& alloc):
                fptr_(other.fptr_), obj_(std::allocator_arg, alloc, std::move(other.obj_))
            {
            }

            [[nodiscard]] Alloc get_allocator() const noexcept { return obj_.get_allocator(); }

            explicit operator bool() const noexcept { return obj_.get() != nullptr; }

            void swap(Self& other) noexcept(alloc_nothrow_swap<Alloc>)
            {
                std::swap(fptr_, other.fptr_);
                obj_.swap(other.obj_);
            }
            friend void swap(Self& lhs, Self& rhs) noexcept { lhs.swap(rhs); }

        protected:
            to_fptr_type<F> fptr_ = nullptr;
            any_sbo_object<Policy, Alloc> obj_;
        };
    } // namespace detail

    template <typename F, typename Policy, typename Alloc = std::allocator<std::byte>>
    class basic_function;

#define CLU_BASIC_FUNC_DEF(cnst, ref, noexc)                                                                           \
    template <typename R, typename... Ts, typename Policy, typename Alloc>                                             \
    class basic_function<R(Ts...) cnst ref noexc, Policy, Alloc>                                                       \
        : public detail::function_base<R(Ts...) cnst ref noexc, Policy, Alloc,                                         \
              basic_function<R(Ts...) cnst ref noexc, Policy, Alloc>>                                                  \
    {                                                                                                                  \
    public:                                                                                                            \
        using detail::function_base<R(Ts...) cnst ref noexc, Policy, Alloc, basic_function>::function_base;            \
        R operator()(Ts... args) cnst ref noexc                                                                        \
        {                                                                                                              \
            return this->fptr_(this->obj_.get(), static_cast<Ts&&>(args)...);                                          \
        }                                                                                                              \
    }

    CLU_BASIC_FUNC_DEF(, , );
    CLU_BASIC_FUNC_DEF(, , noexcept);
    CLU_BASIC_FUNC_DEF(, &, );
    CLU_BASIC_FUNC_DEF(, &, noexcept);
    CLU_BASIC_FUNC_DEF(, &&, );
    CLU_BASIC_FUNC_DEF(, &&, noexcept);
    CLU_BASIC_FUNC_DEF(const, , );
    CLU_BASIC_FUNC_DEF(const, , noexcept);
    CLU_BASIC_FUNC_DEF(const, &, );
    CLU_BASIC_FUNC_DEF(const, &, noexcept);
    CLU_BASIC_FUNC_DEF(const, &&, );
    CLU_BASIC_FUNC_DEF(const, &&, noexcept);

#undef CLU_BASIC_FUNC_DEF

    template <typename F, typename Alloc = std::allocator<std::byte>>
    using function = basic_function<F, copyable_function_policy, Alloc>;
    template <typename F, typename Alloc = std::allocator<std::byte>>
    using move_only_function = basic_function<F, move_only_function_policy, Alloc>;
} // namespace clu
