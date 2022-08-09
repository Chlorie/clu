#pragma once

#include <deque>

#include "../execution/utility.h"

namespace clu::async
{
    namespace buffer_overflow_policies
    {
        // clang-format off
        namespace detail { struct policy_base{}; }

        struct suspend_t final : detail::policy_base {} inline constexpr suspend{};
        struct drop_oldest_t final : detail::policy_base {} inline constexpr drop_oldest{};
        struct drop_latest_t final : detail::policy_base {} inline constexpr drop_latest{};
        // clang-format on
    } // namespace buffer_overflow_policies

    template <typename T>
    concept buffer_overflow_policy = std::derived_from<T, buffer_overflow_policies::detail::policy_base>;

    inline constexpr std::size_t unbounded = static_cast<std::size_t>(-1);

    template <movable_value T, //
        buffer_overflow_policy OverflowPolicy = buffer_overflow_policies::suspend_t, //
        allocator Alloc = std::allocator<T>>
    class channel;

    namespace detail::chnl
    {
        namespace bop = buffer_overflow_policies;

        template <typename T>
        struct empty_buffer
        {
            bool can_push() const noexcept { return false; }
            bool can_pop() const noexcept { return false; }
            [[noreturn]] void push(auto, auto&&) const noexcept { unreachable(); }
            [[noreturn]] T pop() const noexcept { unreachable(); }
            void clear() const noexcept {}
        };

        template <typename T, typename Alloc>
        class bounded_buffer
        {
        public:
            CLU_IMMOVABLE_TYPE(bounded_buffer);

            bounded_buffer(const std::size_t size, const Alloc alloc):
                alloc_(alloc), size_(size), ptr_(alloc_traits::allocate(alloc_, size_))
            {
            }

            ~bounded_buffer() noexcept
            {
                clear();
                alloc_traits::deallocate(alloc_, ptr_, size_);
            }

            bool can_push() const noexcept { return tail_ - head_ < size_; }
            bool can_pop() const noexcept { return tail_ > head_; }

            template <forwarding<T> U>
            void push(bop::suspend_t, U&& value)
            {
                CLU_ASSERT(can_push(), "Trying to push into a full buffer");
                alloc_traits::construct(alloc_, tail_ptr(), static_cast<U&&>(value));
                tail_++;
            }

            template <forwarding<T> U>
            void push(bop::drop_oldest_t, U&& value)
            {
                if (can_push())
                {
                    alloc_traits::construct(alloc_, tail_ptr(), static_cast<U&&>(value));
                    tail_++;
                }
                else
                {
                    ptr_[head_] = static_cast<U&&>(value);
                    if (++head_ >= size_)
                        head_ -= size_;
                    tail_ = head_ + size_;
                }
            }

            template <forwarding<T> U>
            void push(bop::drop_latest_t, U&& value)
            {
                if (can_push())
                {
                    alloc_traits::construct(alloc_, tail_ptr(), static_cast<U&&>(value));
                    tail_++;
                }
                else
                    *tail_ptr() = static_cast<U&&>(value);
            }

            T pop()
            {
                CLU_ASSERT(can_pop(), "Trying to pop from an empty buffer");
                T result = std::move(ptr_[head_]);
                std::destroy_at(ptr_ + head_);
                if (++head_ >= size_)
                {
                    head_ -= size_;
                    tail_ -= size_;
                }
                return result;
            }

            void clear() noexcept
            {
                if constexpr (!std::is_trivially_destructible_v<T>)
                {
                    if (tail_ > size_)
                    {
                        std::destroy(ptr_ + head_, ptr_ + size_);
                        std::destroy(ptr_, ptr_ + (tail_ - size_));
                    }
                    else
                        std::destroy(ptr_ + head_, ptr_ + tail_);
                }
                head_ = tail_ = 0;
            }

        private:
            using alloc_traits = std::allocator_traits<Alloc>;

            CLU_NO_UNIQUE_ADDRESS Alloc alloc_;
            std::size_t size_;
            T* ptr_ = nullptr;
            std::size_t head_ = 0;
            std::size_t tail_ = 0;

            T* tail_ptr() const noexcept
            {
                const std::size_t real_tail = tail_ >= size_ ? tail_ - size_ : tail_;
                return ptr_ + real_tail;
            }
        };

        template <typename T, typename Alloc>
        class unbounded_buffer
        {
        public:
            CLU_IMMOVABLE_TYPE(unbounded_buffer);

            explicit unbounded_buffer(const Alloc alloc): deque_(alloc) {}

            bool can_push() const noexcept { return true; }
            bool can_pop() const noexcept { return !deque_.empty(); }

            template <forwarding<T> U>
            void push(auto, U&& value)
            {
                deque_.push_back(static_cast<U&&>(value));
            }

            T pop()
            {
                CLU_ASSERT(can_pop(), "Trying to pop from an empty buffer");
                T result = std::move(deque_.front());
                deque_.pop_front();
                return result;
            }

            void clear() noexcept { deque_.clear(); }

        private:
            std::deque<T, Alloc> deque_;
        };

        // Type erased (with variant) buffer
        template <typename T, typename Alloc>
        class te_buffer
        {
        public:
            te_buffer(const std::size_t size, const Alloc alloc): data_(te_buffer::make_variant(size, alloc)) {}

            // clang-format off
#define CLU_TE_BUFFER_NO_PARAM_FUNC(type, f, cnst)                                                                    \
    type f() cnst noexcept { return std::visit([](auto& buffer) -> type { return buffer.f(); }, data_); }             \
    static_assert(true)
            // clang-format on

            CLU_TE_BUFFER_NO_PARAM_FUNC(bool, can_push, const);
            CLU_TE_BUFFER_NO_PARAM_FUNC(bool, can_pop, const);
            CLU_TE_BUFFER_NO_PARAM_FUNC(T, pop, );
            CLU_TE_BUFFER_NO_PARAM_FUNC(void, clear, );

#undef CLU_TE_BUFFER_NO_PARAM_FUNC

            template <typename Policy, forwarding<T> U>
            void push(Policy, U&& value)
            {
                std::visit([&](auto& buffer) { buffer.push(Policy{}, static_cast<U&&>(value)); }, data_);
            }

        private:
            using variant = std::variant< //
                empty_buffer<T>, bounded_buffer<T, Alloc>, unbounded_buffer<T, Alloc>>;

            variant data_;

            static variant make_variant(const std::size_t size, const Alloc alloc)
            {
                if (size == 0)
                    return variant(std::in_place_index<0>);
                if (size == unbounded)
                    return variant(std::in_place_index<2>, alloc);
                return variant(std::in_place_index<1>, size, alloc);
            }
        };

        template <typename T>
        class intrusive_queue
        {
        public:
            intrusive_queue() noexcept = default;
            ~intrusive_queue() noexcept = default;

            // clang-format off
            intrusive_queue(intrusive_queue&& other) noexcept:
                head_(std::exchange(other.head_, nullptr)),
                tail_(std::exchange(other.tail_, nullptr)) {}
            // clang-format on

            intrusive_queue& operator=(intrusive_queue&& other) noexcept
            {
                if (&other == this)
                    return *this;
                head_ = std::exchange(other.head_, nullptr);
                tail_ = std::exchange(other.tail_, nullptr);
                return *this;
            }

            void push(T* node) noexcept
            {
                if (!head_)
                    head_ = tail_ = node;
                else
                {
                    tail_->next = node;
                    tail_ = node;
                }
            }

            T* peak() noexcept { return head_; }

            T* pop() noexcept
            {
                if (head_)
                    return std::exchange(head_, head_->next);
                return nullptr;
            }

        private:
            T* head_ = nullptr;
            T* tail_ = nullptr;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        class snd_ops_base
        {
        public:
            snd_ops_base* next = nullptr;
            T value;

            snd_ops_base(channel<T, P, Alloc>* chan, T&& val): value(static_cast<T&&>(val)), chnl_(chan) {}
            CLU_IMMOVABLE_TYPE(snd_ops_base);
            virtual void set_value() noexcept = 0;
            virtual void set_error() noexcept = 0;
            virtual void set_stopped() noexcept = 0;

        protected:
            ~snd_ops_base() noexcept = default;
            void enqueue() noexcept;

        private:
            channel<T, P, Alloc>* chnl_ = nullptr;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        class recv_ops_base
        {
        public:
            recv_ops_base* next = nullptr;
            explicit recv_ops_base(channel<T, P, Alloc>* chan) noexcept: chnl_(chan) {}
            CLU_IMMOVABLE_TYPE(recv_ops_base);
            virtual void set_value(T&& value) noexcept = 0;
            virtual void set_error() noexcept = 0;
            virtual void set_stopped() noexcept = 0;

        protected:
            ~recv_ops_base() noexcept = default;
            void enqueue() noexcept;

        private:
            channel<T, P, Alloc>* chnl_ = nullptr;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc, typename R>
        struct snd_ops_t_
        {
            class type;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc, typename R>
        using snd_ops_t = typename snd_ops_t_<T, P, Alloc, std::decay_t<R>>::type;

        template <typename T, buffer_overflow_policy P, allocator Alloc, typename R>
        class snd_ops_t_<T, P, Alloc, R>::type final : public snd_ops_base<T, P, Alloc>
        {
        public:
            // clang-format off
            template <typename R2>
            type(channel<T, P, Alloc>* chan, T&& val, R2&& recv):
                snd_ops_base<T, P, Alloc>(chan, static_cast<T&&>(val)),
                recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set_value() noexcept override { exec::set_value(static_cast<R&&>(recv_)); }
            void set_error() noexcept override { exec::set_error(static_cast<R&&>(recv_), std::current_exception()); }
            void set_stopped() noexcept override { exec::set_stopped(static_cast<R&&>(recv_)); }

        private:
            CLU_NO_UNIQUE_ADDRESS R recv_;

            friend void tag_invoke(exec::start_t, type& self) noexcept { self.enqueue(); }
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc, typename R>
        struct recv_ops_t_
        {
            class type;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc, typename R>
        using recv_ops_t = typename recv_ops_t_<T, P, Alloc, std::decay_t<R>>::type;

        template <typename T, buffer_overflow_policy P, allocator Alloc, typename R>
        class recv_ops_t_<T, P, Alloc, R>::type final : public recv_ops_base<T, P, Alloc>
        {
        public:
            // clang-format off
            template <typename R2>
            type(channel<T, P, Alloc>* chan, R2&& recv):
                recv_ops_base<T, P, Alloc>(chan),
                recv_(static_cast<R2&&>(recv)) {}
            // clang-format on

            void set_value(T&& value) noexcept override
            {
                exec::set_value(static_cast<R&&>(recv_), static_cast<T&&>(value));
            }

            void set_error() noexcept override { exec::set_error(static_cast<R&&>(recv_), std::current_exception()); }
            void set_stopped() noexcept override { exec::set_stopped(static_cast<R&&>(recv_)); }

        private:
            CLU_NO_UNIQUE_ADDRESS R recv_;

            friend void tag_invoke(exec::start_t, type& self) noexcept { self.enqueue(); }
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        struct snd_snd_t_
        {
            class type;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        using snd_snd_t = typename snd_snd_t_<T, P, Alloc>::type;

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        class snd_snd_t_<T, P, Alloc>::type
        {
        public:
            // clang-format off
            template <forwarding<T> U>
            explicit type(channel<T, P, Alloc>* chan, U&& value) noexcept:
                chnl_(chan), value_(static_cast<U&&>(value)) {}
            // clang-format on

            using completion_signatures = exec::completion_signatures< //
                exec::set_value_t(), exec::set_error_t(std::exception_ptr), exec::set_stopped_t()>;

        private:
            channel<T, P, Alloc>* chnl_;
            T value_;

            template <typename R>
            friend auto tag_invoke(exec::connect_t, type&& self, R&& recv)
            {
                return snd_ops_t<T, P, Alloc, R>( //
                    self.chnl_, static_cast<T&&>(self.value_), static_cast<R&&>(recv));
            }
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        struct recv_snd_t_
        {
            class type;
        };

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        using recv_snd_t = typename recv_snd_t_<T, P, Alloc>::type;

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        class recv_snd_t_<T, P, Alloc>::type
        {
        public:
            explicit type(channel<T, P, Alloc>* chan) noexcept: chnl_(chan) {}

            using completion_signatures = exec::completion_signatures< //
                exec::set_value_t(T), exec::set_error_t(std::exception_ptr), exec::set_stopped_t()>;

        private:
            channel<T, P, Alloc>* chnl_;

            template <typename R>
            friend auto tag_invoke(exec::connect_t, type&& self, R&& recv)
            {
                return recv_ops_t<T, P, Alloc, R>(self.chnl_, static_cast<R&&>(recv));
            }
        };
    } // namespace detail::chnl

    template <movable_value T, buffer_overflow_policy P, allocator Alloc>
    class channel
    {
    public:
        explicit channel(const std::size_t buffer_size, const Alloc alloc = Alloc{}): buffer_(buffer_size, alloc)
        {
            if constexpr (push_never_suspends)
            {
                CLU_ASSERT(buffer_size != 0,
                    "only the \"suspend\" buffer overflow policy can be applied to zero-sized channel ");
            }
        }

        ~channel() noexcept { cancel(); }

        template <forwarding<T> U>
        bool try_send(U&& value)
        {
            std::unique_lock lck(mtx_);
            // Dequeue a pending receiving operation, if there is any
            if (auto* ptr = recv_queue_.pop())
            {
                lck.unlock();
                ptr->set_value(static_cast<U&&>(value));
                return true;
            }
            if constexpr (!push_never_suspends)
                if (!buffer_.can_push()) // The buffer is full, we cannot push
                    return false;
            buffer_.push(P{}, static_cast<U&&>(value));
            return true;
        }

        template <forwarding<T> U>
        [[nodiscard]] auto send_async(U&& value)
        {
            return detail::chnl::snd_snd_t<T, P, Alloc>( //
                this, static_cast<U&&>(value));
        }

        std::optional<T> try_receive()
        {
            std::unique_lock lck(mtx_);
            // Pop a value from the buffer, if there is any
            if (buffer_.can_pop())
            {
                T value = buffer_.pop();
                // Dequeue a sending operation, buffer should be "can_push" since we just popped from it
                if (auto* ptr = snd_queue_.pop())
                    this->push_buffer_release_lock(ptr, lck);
                return std::move(value); // Not pessimization! The return type is optional<T>
            }
            // Nothing in the buffer, dequeue a pending sending operation, if there is any
            if (auto* ptr = snd_queue_.pop())
            {
                lck.unlock();
                scope_exit guard{[=] { ptr->set_value(); }}; // Guard against throwing moves
                return std::move(ptr->value);
            }
            return std::nullopt;
        }

        [[nodiscard]] auto receive_async() { return detail::chnl::recv_snd_t<T, P, Alloc>(this); }

        void cancel() noexcept
        {
            std::unique_lock lck(mtx_);
            buffer_.clear();
            auto sndq = std::move(snd_queue_);
            auto recvq = std::move(recv_queue_);
            lck.unlock(); // Avoid calling arbitrary callback (set_stopped) while holding the lock
            while (auto* ptr = sndq.pop())
                ptr->set_stopped();
            while (auto* ptr = recvq.pop())
                ptr->set_stopped();
        }

    private:
        using snd_ops_base = detail::chnl::snd_ops_base<T, P, Alloc>;
        using recv_ops_base = detail::chnl::recv_ops_base<T, P, Alloc>;
        friend snd_ops_base;
        friend recv_ops_base;

        static constexpr bool push_never_suspends = !std::is_same_v<P, buffer_overflow_policies::suspend_t>;

        std::mutex mtx_;
        detail::chnl::te_buffer<T, Alloc> buffer_;
        detail::chnl::intrusive_queue<snd_ops_base> snd_queue_;
        detail::chnl::intrusive_queue<recv_ops_base> recv_queue_;

        void enqueue_snd_ops(snd_ops_base* ops) noexcept
        {
            std::unique_lock lck(mtx_);
            // Dequeue a pending receiving operation, if there is any
            if (auto* ptr = recv_queue_.pop())
            {
                lck.unlock();
                ptr->set_value(std::move(ops->value));
                ops->set_value();
                return;
            }
            if constexpr (!push_never_suspends)
                if (!buffer_.can_push()) // Buffer is full, enqueue this operation
                {
                    snd_queue_.push(ops);
                    return;
                }
            // Save the value into the buffer
            this->push_buffer_release_lock(ops, lck);
        }

        void enqueue_recv_ops(recv_ops_base* ops) noexcept
        {
            std::unique_lock lck(mtx_);
            // Pop a value from the buffer, if there is any
            if (buffer_.can_pop())
            {
                // Try popping, propagation through the operation state on any error
                try
                {
                    T value = buffer_.pop();
                    // noexcept from now on
                    // Dequeue a sending operation, buffer should be "can_push" since we just popped from it
                    if (auto* ptr = snd_queue_.pop())
                        this->push_buffer_release_lock(ptr, lck);
                    ops->set_value(std::move(value));
                }
                catch (...)
                {
                    lck.unlock();
                    ops->set_error();
                }
            }
            // Nothing in the buffer, dequeue a pending sending operation, if there is any
            else if (auto* ptr = snd_queue_.pop())
            {
                lck.unlock();
                ops->set_value(std::move(ptr->value));
                ptr->set_value();
                return;
            }
            // We can only wait now, enqueue this operation
            else
                recv_queue_.push(ops);
        }

        void push_buffer_release_lock(snd_ops_base* ops, std::unique_lock<std::mutex>& lck) noexcept
        {
            // Try pushing, propagation through the operation state on any error
            try
            {
                buffer_.push(P{}, std::move(ops->value));
            }
            catch (...)
            {
                lck.unlock();
                ops->set_error();
                return;
            }
            lck.unlock();
            ops->set_value();
        }
    };

    template <movable_value T, //
        buffer_overflow_policy P = buffer_overflow_policies::suspend_t, //
        allocator Alloc = std::allocator<T>>
    auto make_channel(const std::size_t buffer_size, P = P{}, const Alloc alloc = Alloc{})
    {
        return channel<T, P, Alloc>(buffer_size, alloc);
    }

    namespace detail::chnl
    {
        template <typename T, buffer_overflow_policy P, allocator Alloc>
        void snd_ops_base<T, P, Alloc>::enqueue() noexcept
        {
            chnl_->enqueue_snd_ops(this);
        }

        template <typename T, buffer_overflow_policy P, allocator Alloc>
        void recv_ops_base<T, P, Alloc>::enqueue() noexcept
        {
            chnl_->enqueue_recv_ops(this);
        }
    } // namespace detail::chnl
} // namespace clu::async
