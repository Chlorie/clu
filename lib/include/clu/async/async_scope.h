#pragma once

#include "async_manual_reset_event.h"

namespace clu
{
    class async_scope;

    namespace detail::async_scp
    {
        class CLU_API stop_token_env
        {
        public:
            explicit stop_token_env(const in_place_stop_token token) noexcept: token_(token) {}

        private:
            in_place_stop_token token_;

            // clang-format off
            CLU_API friend in_place_stop_token tag_invoke(
                exec::get_stop_token_t, const stop_token_env self) noexcept { return self.token_; }
            // clang-format on
        };

        template <typename S>
        concept void_sender = exec::sender_of<S, stop_token_env>;

        class CLU_API recv_base
        {
        public:
            explicit recv_base(async_scope* scope) noexcept: scope_(scope) {}

        protected:
            async_scope* scope_ = nullptr;

            stop_token_env get_env() const noexcept;
            static void decrease_counter(async_scope* scope) noexcept;
        };

        template <typename S>
        struct spawn_recv_t_
        {
            class type;
        };

        template <typename S>
        using spawn_recv_t = typename spawn_recv_t_<S>::type;

        template <typename S>
        struct spawn_ops_wrapper;

        template <typename S>
        class spawn_recv_t_<S>::type : public recv_base
        {
        public:
            type(spawn_ops_wrapper<S>* ops, async_scope* scope) noexcept: recv_base(scope), ops_(ops) {}

        private:
            spawn_ops_wrapper<S>* ops_;

            void finish() const noexcept
            {
                async_scope* scope = scope_; // cache the member
                delete ops_; // this is indirectly destructed here
                decrease_counter(scope);
            }

            friend auto tag_invoke(exec::set_value_t, type&& self) noexcept { self.finish(); }
            friend void tag_invoke(exec::set_error_t, type&&, auto&&) noexcept { std::terminate(); }
            friend auto tag_invoke(exec::set_stopped_t, type&& self) noexcept { self.finish(); }
            friend auto tag_invoke(exec::get_env_t, const type& self) noexcept { return self.get_env(); }
        };

        template <typename S>
        struct spawn_ops_wrapper
        {
            // clang-format off
            template <typename S2>
            explicit spawn_ops_wrapper(S2&& sender, async_scope* scope):
                ops(exec::connect(static_cast<S2&&>(sender), spawn_recv_t<S>(this, scope))) {}
            // clang-format on

            exec::connect_result_t<S, spawn_recv_t<S>> ops;
        };

    } // namespace detail::async_scp

    class CLU_API async_scope
    {
    public:
        async_scope() = default;

        ~async_scope() noexcept
        {
            CLU_ASSERT(count_.load(std::memory_order::relaxed) == 0, //
                "deplete_async() must be awaited before the destruction of async_scope");
        }

        CLU_IMMOVABLE_TYPE(async_scope);

        template <detail::async_scp::void_sender S>
        void spawn(S&& sender)
        {
            using wrapper_t = detail::async_scp::spawn_ops_wrapper<std::decay_t<S>>;
            wrapper_t* ptr = new wrapper_t(static_cast<S&&>(sender), this);
            ev_.reset();
            count_.fetch_add(1, std::memory_order::relaxed);
            exec::start(ptr->ops);
        }

        template <nothrow_invocable Fn>
        void spawn_call(Fn&& func)
        {
            this->spawn(exec::just_from(static_cast<Fn&&>(func)));
        }

        template <typename S>
        auto spawn_future(S&& sender);

        auto deplete_async() { return ev_.wait_async(); }

        void request_stop() noexcept { src_.request_stop(); }
        in_place_stop_token get_stop_token() noexcept { return src_.get_token(); }

    private:
        friend class detail::async_scp::recv_base;

        async_manual_reset_event ev_{true};
        in_place_stop_source src_;
        std::atomic_size_t count_{0};
    };
} // namespace clu
