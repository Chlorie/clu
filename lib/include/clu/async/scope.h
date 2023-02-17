#pragma once

#include <memory>

#include "manual_reset_event.h"
#include "../execution/utility.h"
#include "../execution/algorithms/composed.h"

namespace clu::async
{
    class scope;

    namespace detail::scp
    {
        template <typename A>
        class env
        {
        public:
            // clang-format off
            explicit env(const in_place_stop_token token, A alloc) noexcept:
                token_(token), alloc_(static_cast<A&&>(alloc)) {}
            // clang-format on

        private:
            in_place_stop_token token_;
            CLU_NO_UNIQUE_ADDRESS A alloc_;

            friend auto tag_invoke(get_stop_token_t, const env& self) noexcept { return self.token_; }
            friend A tag_invoke(get_allocator_t, const env& self) noexcept { return self.alloc_; }
        };

        template <typename S, typename A>
        concept void_sender = exec::sender_of<S, env<A>>;

        class recv_base
        {
        public:
            using is_receiver = void;

            explicit recv_base(scope* scope) noexcept: scope_(scope) {}

        protected:
            scope* scope_ = nullptr;

            static void decrease_counter(scope* scope) noexcept;
        };

        template <typename A>
        class recv_alloc_base : public recv_base
        {
        public:
            // clang-format off
            explicit recv_alloc_base(scope* scope, A&& alloc) noexcept:
                recv_base(scope), alloc_(static_cast<A&&>(alloc)) {}
            // clang-format on

        protected:
            CLU_NO_UNIQUE_ADDRESS A alloc_;

            env<A> get_env() const noexcept;
        };

        template <typename S, typename A>
        struct spawn_recv_t_
        {
            class type;
        };

        template <typename S, typename A>
        using spawn_recv_t = typename spawn_recv_t_<S, A>::type;

        template <typename S, typename A>
        struct spawn_ops_wrapper;

        template <typename S, typename A>
        class spawn_recv_t_<S, A>::type : public recv_alloc_base<A>
        {
        public:
            // clang-format off
            type(spawn_ops_wrapper<S, A>* ops, scope* scope, A&& alloc) noexcept:
                recv_alloc_base<A>(scope, static_cast<A&&>(alloc)), ops_(ops) {}
            // clang-format on

        private:
            spawn_ops_wrapper<S, A>* ops_;

            void finish() const noexcept
            {
                using alloc_traits = typename std::allocator_traits<A> //
                    ::template rebind_traits<spawn_ops_wrapper<S, A>>;
                typename alloc_traits::allocator_type rebound = this->alloc_;
                // Caches the members
                scope* scope = this->scope_;
                auto* ops = ops_;
                alloc_traits::destroy(rebound, ops); // *this is indirectly destructed here
                alloc_traits::deallocate(rebound, ops, 1);
                recv_base::decrease_counter(scope);
            }

            friend auto tag_invoke(exec::set_value_t, type&& self) noexcept { self.finish(); }
            friend void tag_invoke(exec::set_error_t, type&&, auto&&) noexcept { std::terminate(); }
            friend auto tag_invoke(exec::set_stopped_t, type&& self) noexcept { self.finish(); }
            friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }
        };

        template <typename S, typename A>
        struct spawn_ops_wrapper
        {
            // clang-format off
            template <typename S2>
            explicit spawn_ops_wrapper(S2&& sender, scope* scope, A&& alloc):
                ops(exec::connect(static_cast<S2&&>(sender),
                    spawn_recv_t<S, A>(this, scope, static_cast<A&&>(alloc)))) {}
            // clang-format on

            exec::connect_result_t<S, spawn_recv_t<S, A>> ops;
        };

        template <typename S, typename A>
        struct future_ops_wrapper;

        template <typename S, typename A>
        struct future_recv_t_
        {
            class type;
        };

        template <typename S, typename A>
        using future_recv_t = typename future_recv_t_<S, A>::type;

        template <typename S, typename A>
        class future_recv_t_<S, A>::type : public recv_alloc_base<A>
        {
        public:
            // clang-format off
            type(std::shared_ptr<future_ops_wrapper<S, A>> ops, scope* scope, A&& alloc) noexcept:
                recv_alloc_base<A>(scope, static_cast<A&&>(alloc)), ops_(std::move(ops)) {}
            // clang-format on

        private:
            std::shared_ptr<future_ops_wrapper<S, A>> ops_;

            template <typename... Ts>
            friend void tag_invoke(exec::set_value_t, type&& self, Ts&&... args) noexcept
            {
                self.set_value(static_cast<Ts&&>(args)...);
            }
            friend void tag_invoke(exec::set_error_t, type&&, auto&&) noexcept { std::terminate(); }
            friend void tag_invoke(exec::set_stopped_t, type&& self) noexcept { self.set_stopped(); }
            friend auto tag_invoke(get_env_t, const type& self) noexcept { return self.get_env(); }

            template <typename... Ts>
            void set_value(Ts&&... args) noexcept;
            void set_stopped() noexcept;

            void finish() noexcept
            {
                scope* scope = this->scope_; // cache the member
                // Our work is done, the eager ops is no longer needed
                // This reset will also delete this, just as a reminder
                ops_->ops.reset();
                recv_base::decrease_counter(scope);
            }
        };

        template <typename S, typename A>
        using value_storage_t = std::optional<exec::value_types_of_t<S, env<A>>>;

        template <typename S, typename A>
        class future_ops_base
        {
        public:
            future_ops_base() = default;
            CLU_IMMOVABLE_TYPE(future_ops_base);
            virtual void set(value_storage_t<S, A>&& value) noexcept = 0;

        protected:
            virtual ~future_ops_base() noexcept = default;
        };

        template <typename S, typename A>
        struct future_ops_wrapper : std::enable_shared_from_this<future_ops_wrapper<S, A>>
        {
            using ops_t = exec::connect_result_t<S, future_recv_t<S, A>>;

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
            value_storage_t<S, A> value;

            template <typename S2>
            void emplace_operation(S2&& snd, scope* scope, A&& alloc)
            {
                ops.emplace_with(
                    [&]
                    {
                        return exec::connect(static_cast<S2&&>(snd), //
                            future_recv_t<S, A>(this->shared_from_this(), scope, static_cast<A&&>(alloc)));
                    });
            }
        };

        template <typename S, typename A>
        template <typename... Ts>
        void future_recv_t_<S, A>::type::set_value(Ts&&... args) noexcept
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
                auto* base = static_cast<future_ops_base<S, A>*>(expected);
                base->set(std::move(ops_->value)); // Set value
            }
            // Or else, let the receiver set itself...
            finish();
        }

        template <typename S, typename A>
        void future_recv_t_<S, A>::type::set_stopped() noexcept
        {
            // Note that the value storage starts as nullopt,
            // so actually we don't need to store anything here.
            if (void* expected = nullptr;
                !ops_->ptr.compare_exchange_strong(expected, ops_.get(), std::memory_order::acq_rel))
            {
                // CAS failed, someone attached a receiver before us
                auto* base = static_cast<future_ops_base<S, A>*>(expected);
                base->set({}); // Set stopped
            }
            finish();
        }

        template <typename S, typename A, typename R>
        struct future_ops_t_
        {
            class type;
        };

        template <typename S, typename A, typename R>
        using future_ops_t = typename future_ops_t_<S, A, std::remove_cvref_t<R>>::type;

        template <typename S, typename A, typename R>
        class future_ops_t_<S, A, R>::type final : public future_ops_base<S, A>
        {
        public:
            // clang-format off
            template <typename R2>
            type(std::shared_ptr<future_ops_wrapper<S, A>> ops, R2&& recv):
                ops_(std::move(ops)), recv_(recv) {}
            // clang-format on

            void set(value_storage_t<S, A>&& value) noexcept override
            {
                if constexpr ( // S does not send value (can only send stopped)
                    exec::value_types_of_t<S, env<A>, meta::constant_q<void>::fn, meta::empty>::value)
                    exec::set_stopped(static_cast<R&&>(recv_)); // NOLINT(bugprone-branch-clone)
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
            std::shared_ptr<future_ops_wrapper<S, A>> ops_;
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

        template <typename S, typename A>
        struct future_snd_t_
        {
            class type;
        };

        template <typename S, typename A>
        using future_snd_t = typename future_snd_t_<std::remove_cvref_t<S>, A>::type;

        template <typename... Ts>
        using set_value_sig = exec::set_value_t(Ts...);

        template <typename S, typename A>
        class future_snd_t_<S, A>::type
        {
        public:
            using is_sender = void;

            // clang-format off
            template <typename S2>
            type(S2&& snd, scope* scope, A&& alloc):
                ops_(std::allocate_shared<future_ops_wrapper<S, A>>(alloc))
            {
                ops_->emplace_operation(static_cast<S2&&>(snd), scope, static_cast<A&&>(alloc));
            }
            // clang-format on

        private:
            friend scope;

            std::shared_ptr<future_ops_wrapper<S, A>> ops_;

            template <typename R>
            friend auto tag_invoke(exec::connect_t, type&& self, R&& recv)
            {
                return future_ops_t<S, A, R>(std::move(self.ops_), static_cast<R&&>(recv));
            }

            // clang-format off
            friend exec::detail::add_sig<
                exec::value_types_of_t<S, env<A>, set_value_sig, exec::completion_signatures>,
                exec::set_stopped_t()
            > tag_invoke(exec::get_completion_signatures_t, const type&, auto&&) { return {}; }
            // clang-format on

            void start_eagerly() noexcept { exec::start(*ops_->ops); }
        };
    } // namespace detail::scp

    class scope
    {
    public:
        scope() = default;
        CLU_IMMOVABLE_TYPE(scope);

        ~scope() noexcept
        {
            CLU_ASSERT(count_.load(std::memory_order::relaxed) == 0, //
                "deplete_async() must be awaited before the destruction of async_scope");
        }

        template <typename S, allocator A = std::allocator<std::byte>>
            requires detail::scp::void_sender<S, A>
        void spawn(S&& sender, A alloc = A{})
        {
            using wrapper_t = detail::scp::spawn_ops_wrapper<std::decay_t<S>, A>;
            using alloc_traits = typename std::allocator_traits<A>::template rebind_traits<wrapper_t>;
            typename alloc_traits::allocator_type rebound = alloc;
            wrapper_t* ptr = alloc_traits::allocate(rebound, 1);
            alloc_traits::construct(rebound, ptr, //
                static_cast<S&&>(sender), this, static_cast<A&&>(alloc));
            // Following operations won't throw
            ev_.reset();
            count_.fetch_add(1, std::memory_order::relaxed);
            exec::start(ptr->ops);
        }

        template <nothrow_invocable Fn, allocator A = std::allocator<std::byte>>
        void spawn_call(Fn&& func, A alloc = A{})
        {
            this->spawn(exec::just_from(static_cast<Fn&&>(func)), static_cast<A&&>(alloc));
        }

        template <exec::sender S, allocator A = std::allocator<std::byte>>
        auto spawn_future(S&& sender, A alloc = A{})
        {
            detail::scp::future_snd_t<S, A> future(static_cast<S&&>(sender), //
                this, static_cast<A&&>(alloc));
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
        friend detail::scp::recv_base;

        manual_reset_event ev_{true};
        in_place_stop_source src_;
        std::atomic_size_t count_{0};
    };

    namespace detail::scp
    {
        template <typename A>
        env<A> recv_alloc_base<A>::get_env() const noexcept
        {
            return env(scope_->get_stop_token(), alloc_);
        }
    } // namespace detail::scp
} // namespace clu::async
