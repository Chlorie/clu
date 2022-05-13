#pragma once

#include "async_manual_reset_event.h"

namespace clu
{
    class async_scope;

    CLU_SUPPRESS_EXPORT_WARNING
    namespace detail::async_scp
    {
        class CLU_API stop_token_env
        {
        public:
            explicit stop_token_env(const in_place_stop_token token) noexcept: token_(token) {}

        private:
            in_place_stop_token token_;

            // clang-format off
            friend in_place_stop_token tag_invoke(
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
                async_scope* scope = this->scope_; // cache the member
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

        template <typename S>
        struct future_ops_wrapper;

        template <typename S>
        struct future_recv_t_
        {
            class type;
        };

        template <typename S>
        using future_recv_t = typename future_recv_t_<S>::type;

        template <typename S>
        class future_recv_t_<S>::type : recv_base
        {
        public:
            // clang-format off
            type(async_scope* scope, std::shared_ptr<future_ops_wrapper<S>> ops) noexcept:
                recv_base(scope), ops_(std::move(ops)) {}
            // clang-format on

        private:
            std::shared_ptr<future_ops_wrapper<S>> ops_;

            template <typename... Ts>
            friend void tag_invoke(exec::set_value_t, type&& self, Ts&&... args) noexcept
            {
                self.set_value(static_cast<Ts&&>(args)...);
            }
            friend void tag_invoke(exec::set_error_t, type&&, auto&&) noexcept { std::terminate(); }
            friend void tag_invoke(exec::set_stopped_t, type&& self) noexcept { self.set_stopped(); }
            friend auto tag_invoke(exec::get_env_t, const type& self) noexcept { return self.get_env(); }

            template <typename... Ts>
            void set_value(Ts&&... args) noexcept;
            void set_stopped() noexcept;

            void finish() noexcept
            {
                async_scope* scope = this->scope_; // cache the member
                // Our work is done, the eager ops is no longer needed
                // This reset will also delete this, just as a reminder
                ops_->ops.reset();
                decrease_counter(scope);
            }
        };

        template <typename S>
        using value_storage_t = std::optional<exec::value_types_of_t<S, stop_token_env>>;

        template <typename S>
        class future_ops_base
        {
        public:
            future_ops_base() = default;
            CLU_IMMOVABLE_TYPE(future_ops_base);
            virtual void set(value_storage_t<S>&& value) noexcept = 0;

        protected:
            virtual ~future_ops_base() noexcept = default;
        };

        template <typename S>
        struct future_ops_wrapper : std::enable_shared_from_this<future_ops_wrapper<S>>
        {
            using ops_t = exec::connect_result_t<S, future_recv_t<S>>;

            // ptr == nullptr:
            //    The value is not there, nor is an operation state
            //    corresponding to the future sender started.
            // ptr == this:
            //    The value is there, set by the eagerly started this->ops
            //    contained in *this. Later started operation state
            //    on the future sender should not register the receiver
            //    callback, instead it should just grab this->value here.
            // ptr == something else:
            //    The pointer should be a future_ops_base<S>*, a receiver
            //    is registered. After this->ops sets the value it should
            //    callback the receiver.
            std::atomic<void*> ptr{nullptr};

            exec::detail::ops_optional<ops_t> ops;
            value_storage_t<S> value;

            template <typename S2>
            void emplace_operation(async_scope* scope, S2&& snd)
            {
                ops.emplace_with(
                    [&]
                    {
                        return exec::connect(static_cast<S2&&>(snd), //
                            future_recv_t<S>(scope, this->shared_from_this()));
                    });
            }
        };

        template <typename S>
        template <typename... Ts>
        void future_recv_t_<S>::type::set_value(Ts&&... args) noexcept
        {
            try
            {
                ops_->value.emplace( //
                    std::in_place_type<std::tuple<std::decay_t<Ts>...>>, //
                    static_cast<Ts&&>(args)...);
            }
            catch (...) // Say no to exceptions
            {
                std::terminate();
            }
            if (void* expected = nullptr;
                !ops_->ptr.compare_exchange_strong(expected, ops_.get(), std::memory_order::acq_rel))
            {
                // CAS failed, someone attached a receiver before us
                auto* base = static_cast<future_ops_base<S>*>(expected);
                base->set(std::move(ops_->value)); // Set value
            }
            // Or else, let the receiver set itself...
            finish();
        }

        template <typename S>
        void future_recv_t_<S>::type::set_stopped() noexcept
        {
            // Note that the value storage starts as nullopt,
            // so actually we don't need to store anything here.
            if (void* expected = nullptr;
                !ops_->ptr.compare_exchange_strong(expected, ops_.get(), std::memory_order::acq_rel))
            {
                // CAS failed, someone attached a receiver before us
                auto* base = static_cast<future_ops_base<S>*>(expected);
                base->set({}); // Set stopped
            }
            finish();
        }

        template <typename S, typename R>
        struct future_ops_t_
        {
            class type;
        };

        template <typename S, typename R>
        using future_ops_t = typename future_ops_t_<S, std::remove_cvref_t<R>>::type;

        template <typename S, typename R>
        class future_ops_t_<S, R>::type final : public future_ops_base<S>
        {
        public:
            // clang-format off
            template <typename R2>
            type(std::shared_ptr<future_ops_wrapper<S>> ops, R2&& recv):
                ops_(std::move(ops)), recv_(recv) {}
            // clang-format on

            void set(value_storage_t<S>&& value) noexcept override
            {
                if constexpr ( // S does not send value (can only send stopped)
                    exec::value_types_of_t<S, stop_token_env, meta::constant_q<void>::fn, meta::empty>::value)
                    exec::set_stopped(static_cast<R&&>(recv_));
                else if (value.has_value()) // S sent value
                    std::visit(
                        [&]<typename Tup>(Tup&& tuple)
                        {
                            std::apply([&]<typename... Ts>(Ts&&... args)
                                { exec::set_value(static_cast<R&&>(recv_), static_cast<Ts&&>(args)...); },
                                static_cast<Tup&&>(tuple));
                        },
                        std::move(*value));
                else // S did not send value
                    exec::set_stopped(static_cast<R&&>(recv_));
            }

        private:
            std::shared_ptr<future_ops_wrapper<S>> ops_;
            R recv_;

            friend void tag_invoke(exec::start_t, type& self) noexcept
            {
                if (void* expected = nullptr;
                    !self.ops_->ptr.compare_exchange_strong(expected, &self, std::memory_order::acq_rel))
                {
                    // We lost the race, set the receiver ourselves
                    self.set(std::move(self.ops_->value));
                }
            }
        };

        template <typename S>
        struct future_snd_t_
        {
            class type;
        };

        template <typename S>
        using future_snd_t = typename future_snd_t_<std::remove_cvref_t<S>>::type;

        template <typename... Ts>
        using set_value_sig = exec::set_value_t(Ts...);

        template <typename S>
        class future_snd_t_<S>::type
        {
        public:
            // clang-format off
            template <typename S2>
            type(async_scope* scope, S2&& snd):
                ops_(std::make_shared<future_ops_wrapper<S>>())
            {
                ops_->emplace_operation(scope, static_cast<S2&&>(snd));
            }
            // clang-format on

        private:
            friend async_scope;

            std::shared_ptr<future_ops_wrapper<S>> ops_;

            template <typename R>
            friend auto tag_invoke(exec::connect_t, type&& self, R&& recv)
            {
                return future_ops_t<S, R>(std::move(self.ops_), static_cast<R&&>(recv));
            }

            // clang-format off
            friend exec::detail::add_sig<
                exec::value_types_of_t<S, stop_token_env, set_value_sig, exec::completion_signatures>,
                exec::set_stopped_t()
            > tag_invoke(exec::get_completion_signatures_t, const type&, auto&&) { return {}; }
            // clang-format on

            void start_eagerly() noexcept { exec::start(*ops_->ops); }
        };
    } // namespace detail::async_scp

    class CLU_API async_scope
    {
    public:
        async_scope() = default;
        CLU_IMMOVABLE_TYPE(async_scope);

        ~async_scope() noexcept
        {
            CLU_ASSERT(count_.load(std::memory_order::relaxed) == 0, //
                "deplete_async() must be awaited before the destruction of async_scope");
        }

        template <detail::async_scp::void_sender S>
        void spawn(S&& sender)
        {
            using wrapper_t = detail::async_scp::spawn_ops_wrapper<std::decay_t<S>>;
            wrapper_t* ptr = new wrapper_t(static_cast<S&&>(sender), this);
            // Following operations won't throw
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
        auto spawn_future(S&& sender)
        {
            detail::async_scp::future_snd_t<S> future(this, static_cast<S&&>(sender));
            // Following operations won't throw
            ev_.reset();
            count_.fetch_add(1, std::memory_order::relaxed);
            future.start_eagerly();
            return std::move(future);
        }

        [[nodiscard]] auto deplete_async() { return ev_.wait_async(); }

        void request_stop() noexcept { src_.request_stop(); }
        in_place_stop_token get_stop_token() noexcept { return src_.get_token(); }

    private:
        friend class detail::async_scp::recv_base;

        async_manual_reset_event ev_{true};
        in_place_stop_source src_;
        std::atomic_size_t count_{0};
    };
    CLU_RESTORE_EXPORT_WARNING
} // namespace clu
