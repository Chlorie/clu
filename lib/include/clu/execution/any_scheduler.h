#pragma once

#include <utility>
#include "execution_traits.h"

namespace clu::exec
{
    namespace detail::any_schd
    {
        // Type-erased operation state
        // Accommodates the state produced by connecting the (non-type-erased) sender
        // to the proxy receiver
        class any_ops_t
        {
        public:
            // clang-format off
            template <callable Fn>
            explicit any_ops_t(Fn&& fn):
                ptr_(new call_result_t<Fn>(fn())),
                destruct_([](void* ptr) noexcept { delete static_cast<call_result_t<Fn>*>(ptr); }),
                start_([](void* ptr) noexcept { exec::start(*static_cast<call_result_t<Fn>*>(ptr)); }) {}
            // clang-format on

            CLU_IMMOVABLE_TYPE(any_ops_t);
            ~any_ops_t() noexcept { destruct_(ptr_); }

        private:
            void* ptr_ = nullptr;
            void (*destruct_)(void*) noexcept = nullptr;
            void (*start_)(void*) noexcept = nullptr;

            friend void tag_invoke(start_t, const any_ops_t& self) noexcept { self.start_(self.ptr_); }
        };

        struct recv_vtbl
        {
            void (*set_value)(void*) noexcept = nullptr;
            void (*set_error)(void*, const std::exception_ptr&) noexcept = nullptr;
            void (*set_stopped)(void*) noexcept = nullptr;
        };

        template <typename R>
        R&& as_rvalue(void* ptr) noexcept
        {
            return static_cast<R&&>(*static_cast<R*>(ptr));
        }

        template <typename R>
        constexpr recv_vtbl recv_vtbl_for = {
            .set_value = [](void* ptr) noexcept { exec::set_value(as_rvalue<R>(ptr)); },
            .set_error = [](void* ptr, const std::exception_ptr& eptr) noexcept
            { exec::set_error(as_rvalue<R>(ptr), eptr); },
            .set_stopped = [](void* ptr) noexcept { exec::set_stopped(as_rvalue<R>(ptr)); } //
        };

        struct env_t
        {
            in_place_stop_token token;
            friend auto tag_invoke(get_stop_token_t, const env_t self) noexcept { return self.token; }
        };

        // This is a proxy (type-erased reference) to a real receiver
        class proxy_recv_t
        {
        public:
            using is_receiver = void;

            // clang-format off
            template <typename R>
            proxy_recv_t(R& recv, const in_place_stop_token token):
                ptr_(std::addressof(recv)),
                vfptr_(&recv_vtbl_for<R>),
                token_(token) {}
            // clang-format on

        private:
            void* ptr_ = nullptr;
            const recv_vtbl* vfptr_ = nullptr;
            in_place_stop_token token_;

            friend void tag_invoke(set_value_t, proxy_recv_t&& self) noexcept { self.vfptr_->set_value(self.ptr_); }
            template <typename E>
            friend void tag_invoke(set_error_t, proxy_recv_t&& self, E&& error) noexcept
            {
                self.vfptr_->set_error(self.ptr_, detail::make_exception_ptr(static_cast<E&&>(error)));
            }
            friend void tag_invoke(set_stopped_t, proxy_recv_t&& self) noexcept { self.vfptr_->set_stopped(self.ptr_); }
            friend env_t tag_invoke(get_env_t, const proxy_recv_t& self) noexcept { return {self.token_}; }
        };

        struct schd_base
        {
            virtual ~schd_base() noexcept = default;
            virtual bool equal(const schd_base& other) noexcept = 0;
            virtual any_ops_t connect(proxy_recv_t&& recv) = 0;
        };

        template <typename Schd>
        class schd_impl final : public schd_base
        {
        public:
            // clang-format off
            template <typename Schd2>
            explicit schd_impl(Schd2&& schd): schd_(static_cast<Schd2&&>(schd)) {}
            // clang-format on

            bool equal(const schd_base& other) noexcept override
            {
                if (const auto* other_ptr = dynamic_cast<const schd_impl*>(&other))
                    return schd_ == other_ptr->schd_;
                return false;
            }

            any_ops_t connect(proxy_recv_t&& recv) override
            {
                return any_ops_t(
                    [&]
                    {
                        return exec::connect(exec::schedule(schd_), //
                            static_cast<proxy_recv_t&&>(recv));
                    });
            }

        private:
            Schd schd_;
        };

        template <typename S>
        auto make_schd_ptr(S&& schd)
        {
            return std::make_shared<schd_impl<std::decay_t<S>>>(static_cast<S&&>(schd));
        }

        template <typename R>
        struct ops_t_
        {
            class type;
        };

        template <typename R>
        using ops_t = typename ops_t_<std::decay_t<R>>::type;

        // in place case
        template <typename R>
        struct stop_propagator
        {
            R recv;
            in_place_stop_token get_token() const noexcept { return get_stop_token(get_env(recv)); }
        };

        template <typename R>
            requires unstoppable_token<stop_token_of_t<env_of_t<R>>>
        struct stop_propagator<R>
        {
            R recv;
            in_place_stop_token get_token() const noexcept { return {}; }
        };

        template <typename T>
        concept non_in_place_stoppable_token = //
            (!std::same_as<T, in_place_stop_token>)&& //
            (!unstoppable_token<T>)&& //
            stoppable_token<T>;

        template <typename R>
            requires non_in_place_stoppable_token<stop_token_of_t<env_of_t<R>>>
        struct stop_propagator<R>
        {
            struct callback_t
            {
                in_place_stop_source& src;
                void operator()() const noexcept { src.request_stop(); }
            };

            R recv;
            in_place_stop_source src;
            typename stop_token_of_t<env_of_t<R>>::template callback_type<callback_t> callback{src};

            in_place_stop_token get_token() noexcept { return src.get_token(); }
        };

        // The real operation state returned by connecting the type-erased sender
        // to a non-type-erased receiver
        template <typename R>
        class ops_t_<R>::type
        {
        public:
            // clang-format off
            template <typename R2>
            type(schd_base& schd, R2&& recv):
                prop_{.recv = static_cast<R2&&>(recv)},
                ops_{schd.connect(proxy_recv_t(prop_.recv, prop_.get_token()))} {}
            // clang-format on

        private:
            stop_propagator<R> prop_;
            any_ops_t ops_;

            friend void tag_invoke(start_t, type& self) noexcept { exec::start(self.ops_); }
        };
    } // namespace detail::any_schd

    class any_scheduler
    {
    public:
        constexpr any_scheduler() noexcept = default;
        ~any_scheduler() noexcept = default;
        any_scheduler(const any_scheduler& other) noexcept = default;
        any_scheduler(any_scheduler&& other) noexcept = default;
        any_scheduler& operator=(const any_scheduler& other) noexcept = default;
        any_scheduler& operator=(any_scheduler&& other) noexcept = default;

        // clang-format off
        template <typename Schd> requires
            (!similar_to<Schd, any_scheduler>) &&
            scheduler<Schd>
        explicit(false) any_scheduler(Schd&& schd):
            ptr_(detail::any_schd::make_schd_ptr(static_cast<Schd&&>(schd))) {}
        // clang-format on

        void swap(any_scheduler& other) noexcept { ptr_.swap(other.ptr_); }
        friend void swap(any_scheduler& lhs, any_scheduler& rhs) noexcept { lhs.ptr_.swap(rhs.ptr_); }

        // clang-format off
        template <scheduler Schd> const Schd& get() const { return dynamic_cast<const Schd&>(*ptr_); }
        template <scheduler Schd> const Schd* get_if() const noexcept { return dynamic_cast<const Schd*>(&*ptr_); }
        // clang-format on

        friend bool operator==(const any_scheduler& lhs, const any_scheduler& rhs) noexcept
        {
            return lhs.ptr_->equal(*rhs.ptr_);
        }

    private:
        using pointer_t = std::shared_ptr<detail::any_schd::schd_base>;
        pointer_t ptr_;

        class snd_t
        {
        public:
            using is_sender = void;

            explicit snd_t(const pointer_t& ptr) noexcept: ptr_(ptr) {}

        private:
            pointer_t ptr_;

            any_scheduler get_schd() const noexcept
            {
                any_scheduler schd;
                schd.ptr_ = ptr_;
                return schd;
            }

            template <typename R>
            friend auto tag_invoke(connect_t, const snd_t& self, R&& recv)
            {
                return detail::any_schd::ops_t<R>( //
                    *self.ptr_, static_cast<R&&>(recv));
            }

            // clang-format off
            friend completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>
            tag_invoke(get_completion_signatures_t, const snd_t&, auto&&) noexcept { return {}; }
            // clang-format on

            friend auto tag_invoke(get_env_t, const snd_t& self)
            {
                return adapt_env(empty_env{}, //
                    query_value{get_completion_scheduler<set_value_t>, self.get_schd()});
            }
        };

        // clang-format off
        // Make this into a template to avoid infinite recursion of concept evaluation when
        // ADL of any_scheduler is triggered.
        friend auto tag_invoke(schedule_t, //
            const std::same_as<any_scheduler> auto& self) noexcept { return snd_t(self.ptr_); }
        // clang-format on
    };
} // namespace clu::exec
