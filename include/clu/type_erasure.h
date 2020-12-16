#pragma once

#include <memory>
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

        template <typename Model>
        struct vtable
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
            void (*dtor)(void*);
            vfptrs_t vfptrs;

            template <typename T>
            static constexpr vtable generate_for()
            {
                return {
                    [](void* this_) { delete static_cast<T*>(this_); },
                    gen_vfptrs<T>()
                };
            }
        };

        template <typename Model, typename T>
        inline constexpr vtable<Model> vtable_for = vtable<Model>::template generate_for<T>();

        template <typename Model, bool Nullable>
        class dispatch_base
        {
        private:
            void* ptr_ = nullptr;
            const vtable<Model>* vtable_ = nullptr;

        public:
            dispatch_base() noexcept = default;
            dispatch_base(void* ptr, const vtable<Model>* vtable): ptr_(ptr), vtable_(vtable) {}

            dispatch_base(const dispatch_base&) = delete;
            dispatch_base& operator=(const dispatch_base&) = delete;

            ~dispatch_base() noexcept { if (ptr_) vtable_->dtor(ptr_); }

            dispatch_base(dispatch_base&& other) noexcept:
                ptr_(take(other.ptr_)), vtable_(take(other.vtable_)) {}

            dispatch_base& operator=(dispatch_base&& other) noexcept
            {
                if (&other != this)
                {
                    ptr_ = take(other.ptr_);
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

    template <typename Model, bool Nullable = false>
    class type_erased : public Model::template interface<detail::dispatch_base<Model, Nullable>>
    {
        static_assert(detail::has_members_v<detail::prototype<Model>, Model::template members>,
            "Interface of the model type should implement the specified members");

    private:
        using indirect_base = detail::dispatch_base<Model, Nullable>;
        using base = typename Model::template interface<indirect_base>;

    public:
        type_erased() noexcept requires Nullable = default;

        template <typename T> requires (detail::has_members_v<T, Model::template members>, !forwarding<T, type_erased>)
        explicit(false) type_erased(T&& value): base{
            indirect_base(
                new std::remove_cv_t<T>(std::forward<T>(value)),
                &detail::vtable_for<Model, T>
            )
        } {}
    };
}
