#pragma once

#include <tuple>
#include <clu/take.h>
#include <clu/concepts.h>
#include <clu/function_traits.h>

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

        struct nullable : policy_base {};
        struct copyable : policy_base {};
    }

    namespace detail
    {
        template <typename Impl, template <typename> typename Members, typename Sfinae = void>
        struct has_members : std::false_type {};
        template <typename Impl, template <typename> typename Members>
        struct has_members<Impl, Members, std::void_t<Members<Impl>>> : std::true_type {};

        template <typename Impl, template <typename> typename Members>
        inline constexpr bool has_members_v = has_members<Impl, Members>::value;

        struct omnipotype
        {
            template <typename T>
            explicit(false) operator T() const
            {
                return std::declval<T>();
            }
        };

        template <typename Model>
        using prototype = typename Model::template interface<meta::empty_type_list>;

        // @formatter:off
        template <bool Copyable> struct copy_vfptr_provider {};
        template <> struct copy_vfptr_provider<true> { void* (*copy_ctor)(void*) = nullptr; };
        // @formatter:on

        template <typename Model, bool Copyable>
        struct vtable : copy_vfptr_provider<Copyable>
        {
        private:
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
            void (*dtor)(void*) noexcept = nullptr;
            vfptrs_t vfptrs;

            template <typename T>
            static constexpr vtable generate_for()
            {
                vtable result;
                if constexpr (Copyable)
                    result.copy_ctor = [](void* this_) -> void* { return new T(*static_cast<T*>(this_)); };
                result.dtor = [](void* this_) noexcept { delete static_cast<T*>(this_); };
                result.vfptrs = gen_vfptrs<T>();
                return result;
            }
        };

        template <typename Vtable, typename T>
        inline constexpr Vtable vtable_for = Vtable::template generate_for<T>();

        template <typename Model, type_erasure_policy::policy... Policies>
        class dispatch_base
        {
        protected:
            static constexpr bool copyable =
                (std::is_same_v<Policies, type_erasure_policy::copyable> || ...);
            using vtable_t = vtable<Model, copyable>;

            void reset() noexcept
            {
                clean_up();
                ptr_ = nullptr;
                vtable_ = nullptr;
            }

        private:
            void* ptr_ = nullptr;
            const vtable_t* vtable_ = nullptr;

            void clean_up() noexcept { if (ptr_) vtable_->dtor(ptr_); }

        public:
            dispatch_base() noexcept = default;
            dispatch_base(void* ptr, const vtable_t* vtable): ptr_(ptr), vtable_(vtable) {}

            dispatch_base(const dispatch_base&) = delete;
            dispatch_base& operator=(const dispatch_base&) = delete;

            dispatch_base(const dispatch_base& other) requires copyable:
                ptr_(other.vtable_->copy_ctor(other.ptr_)),
                vtable_(other.vtable_) {}

            dispatch_base& operator=(const dispatch_base& other) requires copyable
            {
                if (&other != this)
                {
                    clean_up();
                    ptr_ = other.vtable_->copy_ctor(other.ptr_);
                    vtable_ = other.vtable_;
                }
                return *this;
            }

            ~dispatch_base() noexcept { clean_up(); }

            dispatch_base(dispatch_base&& other) noexcept:
                ptr_(take(other.ptr_)), vtable_(take(other.vtable_)) {}

            dispatch_base& operator=(dispatch_base&& other) noexcept
            {
                if (&other != this)
                {
                    clean_up();
                    ptr_ = take(other.ptr_);
                    vtable_ = take(other.vtable_);
                }
                return *this;
            }

            [[nodiscard]] bool empty() const noexcept { return ptr_ != nullptr; }
            [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }
            [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return ptr_ == nullptr; }

            template <size_t I, typename This, typename... Ts>
            friend decltype(auto) vdispatch_impl(This&& this_, Ts&&... args); // NOLINT
        };

        template <size_t I, typename This, typename... Ts>
        decltype(auto) vdispatch_impl(This&& this_, Ts&&... args)
        {
            return get<I>(this_.vtable_->vfptrs)(
                this_.ptr_, std::forward<Ts>(args)...);
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

        static constexpr bool nullable =
            (std::is_same_v<Policies, type_erasure_policy::nullable> || ...);
        static constexpr bool copyable = indirect_base::copyable;

        template <typename T>
        static constexpr bool implements_interface =
            detail::has_members_v<std::remove_cvref_t<T>, Model::template members> &&
            (!copyable || std::is_copy_constructible_v<T>);

    public:
        type_erased() = delete;
        type_erased() noexcept requires nullable = default;

        template <typename T> requires (implements_interface<T> && !forwarding<T, type_erased>)
        explicit(false) type_erased(T&& value): base{
            indirect_base(
                new std::remove_cv_t<T>(std::forward<T>(value)),
                &detail::vtable_for<typename indirect_base::vtable_t, T>
            )
        } {}
        
        void reset() noexcept requires nullable { indirect_base::reset(); }
    };
}
