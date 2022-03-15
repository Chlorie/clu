#pragma once

#include <compare>

#include "concepts.h"
#include "concurrency.h"

namespace clu
{
    namespace detail
    {
        template <template <typename> typename>
        struct check_type_alias_exists
        {
        };
    } // namespace detail

    // clang-format off
    template <typename T>
    concept stoppable_token =
        std::copy_constructible<T> &&
        std::move_constructible<T> &&
        std::is_nothrow_copy_constructible_v<T> &&
        std::is_nothrow_move_constructible_v<T> &&
        std::equality_comparable<T> &&
        requires(const T& token)
        {
            { token.stop_requested() } noexcept -> boolean_testable;
            { token.stop_possible() } noexcept -> boolean_testable;
            typename detail::check_type_alias_exists<T::template callback_type>;
        };

    template <typename Token, typename Callback, typename Init = Callback>
    concept stoppable_token_for =
        stoppable_token<Token> &&
        std::invocable<Callback> &&
        requires { typename Token::template callback_type<Callback>; } &&
        std::constructible_from<Callback, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, Token, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, Token&, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, const Token, Init> &&
        std::constructible_from<typename Token::template callback_type<Callback>, const Token&, Init>;

    template <typename T>
    concept unstoppable_token =
        stoppable_token<T> &&
        requires
        {
            { T::stop_possible() } -> boolean_testable;
        } &&
        (!T::stop_possible());
    // clang-format on

    class never_stop_token
    {
    private:
        struct callback
        {
            explicit callback(never_stop_token, auto&&) noexcept {}
        };

    public:
        template <typename>
        using callback_type = callback;
        [[nodiscard]] static constexpr bool stop_possible() noexcept { return false; }
        [[nodiscard]] static constexpr bool stop_requested() noexcept { return false; }
        [[nodiscard]] friend constexpr bool operator==(never_stop_token, never_stop_token) noexcept = default;
    };

    template <typename Callback>
    class in_place_stop_callback;
    class in_place_stop_source;
    class in_place_stop_token;

    namespace detail
    {
        class in_place_stop_cb_base
        {
        protected:
            using callback_t = void (*)(in_place_stop_cb_base*) noexcept;

        public:
            in_place_stop_cb_base(in_place_stop_source* src, const callback_t callback) noexcept:
                src_(src), callback_(callback)
            {
            }

        protected:
            friend in_place_stop_source;

            in_place_stop_source* src_ = nullptr;
            callback_t callback_ = nullptr;
            in_place_stop_cb_base* prev_ = nullptr;
            in_place_stop_cb_base* next_ = nullptr;
            std::atomic_uint8_t state_{};
            bool removed_during_exec_ = false;

            void execute() noexcept { callback_(this); }

            void attach() noexcept;
            void detach() noexcept;
        };
    } // namespace detail

    class in_place_stop_source
    {
    public:
        constexpr in_place_stop_source() noexcept = default;

        in_place_stop_source(in_place_stop_source&&) noexcept = delete;

        ~in_place_stop_source() noexcept
        {
            CLU_ASSERT(callbacks_.unsafe_load_relaxed() == nullptr,
                "in_place_stop_source destroyed before the callbacks are done");
        }

        [[nodiscard]] in_place_stop_token get_token() noexcept;
        [[nodiscard]] bool stop_possible() const noexcept { return !stop_requested(); }
        [[nodiscard]] bool stop_requested() const noexcept { return requested_.load(std::memory_order::acquire); }

    private:
        auto lock_if_not_requested() noexcept
        {
            struct result
            {
                bool locked = false;
                detail::in_place_stop_cb_base* head = nullptr;
            };
            if (stop_requested())
                return result{}; // Fast path
            auto* head = callbacks_.lock_and_load();
            // Check again in case someone changed the state
            // while we're trying to acquire the lock
            if (stop_requested())
            {
                callbacks_.store_and_unlock(head);
                return result{};
            }
            return result{true, head};
        }

    public:
        bool request_stop() noexcept
        {
            auto [locked, current] = lock_if_not_requested();
            if (!locked)
                return false;
            requesting_thread_ = std::this_thread::get_id();
            requested_.store(true, std::memory_order::release);
            while (current)
            {
                // Detach the first callback
                auto* new_head = current->next_;
                if (new_head)
                    new_head->prev_ = nullptr;
                current->state_.store(true, std::memory_order::release);
                callbacks_.store_and_unlock(new_head);
                // Now that we have released the lock, start executing the callback
                current->execute();
                if (!current->removed_during_exec_) // detach() not called during execute()
                {
                    // Change the state and notify
                    current->state_.store(completed, std::memory_order::release);
                    current->state_.notify_one();
                }
                current = callbacks_.lock_and_load();
            }
            callbacks_.store_and_unlock(nullptr);
            return true;
        }

    private:
        friend detail::in_place_stop_cb_base;

        static constexpr std::uint8_t not_started = 0;
        static constexpr std::uint8_t started = 1;
        static constexpr std::uint8_t completed = 2;

        std::atomic_bool requested_{};
        locked_ptr<detail::in_place_stop_cb_base> callbacks_{};
        std::thread::id requesting_thread_{}; // The thread which requested stop

        bool try_attach(detail::in_place_stop_cb_base* cb) noexcept
        {
            const auto [locked, head] = lock_if_not_requested();
            if (!locked)
                return false;
            // Add the new one before the current head
            cb->next_ = head;
            if (head)
                head->prev_ = cb;
            callbacks_.store_and_unlock(cb);
            return true;
        }

        void detach(detail::in_place_stop_cb_base* cb) noexcept
        {
            auto* head = callbacks_.lock_and_load();
            if (cb->state_.load(std::memory_order::acquire) == not_started)
            {
                // Hasn't started executing
                if (cb->next_)
                    cb->next_->prev_ = cb->prev_;
                if (cb->prev_) // In list
                {
                    cb->prev_->next_ = cb->next_;
                    callbacks_.store_and_unlock(head);
                }
                else // Is head of list
                    callbacks_.store_and_unlock(cb->next_);
            }
            else
            {
                // Already started executing
                const auto id = requesting_thread_;
                callbacks_.store_and_unlock(head); // Just unlock, we won't modify the linked list
                if (std::this_thread::get_id() == id) // execute() called detach()
                    cb->removed_during_exec_ = true;
                else // Executing on a different thread
                    cb->state_.wait(started, std::memory_order::acquire); // Wait until the callback completes
            }
        }
    };

    namespace detail
    {
        inline void in_place_stop_cb_base::attach() noexcept
        {
            if (!src_->try_attach(this))
            {
                // Already locked, stop requested
                src_ = nullptr;
                execute();
            }
        }

        inline void in_place_stop_cb_base::detach() noexcept
        {
            if (src_)
                src_->detach(this);
        }
    } // namespace detail

    class in_place_stop_token
    {
    public:
        template <typename Callback>
        using callback_type = in_place_stop_callback<Callback>;

        constexpr in_place_stop_token() noexcept = default;
        void swap(in_place_stop_token& other) noexcept { std::swap(src_, other.src_); }
        friend void swap(in_place_stop_token& lhs, in_place_stop_token& rhs) noexcept { std::swap(lhs.src_, rhs.src_); }

        [[nodiscard]] bool stop_requested() const noexcept { return src_ != nullptr && src_->stop_requested(); }
        [[nodiscard]] bool stop_possible() const noexcept { return src_ != nullptr; }

        [[nodiscard]] bool operator==(const in_place_stop_token&) const noexcept = default;

    private:
        friend class in_place_stop_source;
        template <typename Callback>
        friend class in_place_stop_callback;

        in_place_stop_source* src_ = nullptr;
        explicit in_place_stop_token(in_place_stop_source* src) noexcept: src_(src) {}
    };

    inline in_place_stop_token in_place_stop_source::get_token() noexcept { return in_place_stop_token(this); }

    template <typename Callback>
    class in_place_stop_callback : public detail::in_place_stop_cb_base
    {
    public:
        using callback_type = Callback;

        template <class C>
        explicit in_place_stop_callback(const in_place_stop_token st, C&& cb) noexcept(
            std::is_nothrow_constructible_v<Callback, C>):
            in_place_stop_cb_base(st.src_, &callback_wrap),
            callback_(static_cast<C&&>(cb))
        {
            this->attach();
        }

        ~in_place_stop_callback() noexcept { this->detach(); }

        in_place_stop_callback(in_place_stop_callback&&) = delete;

    private:
        Callback callback_;

        static void callback_wrap(in_place_stop_cb_base* base) noexcept
        {
            std::move(static_cast<in_place_stop_callback*>(base)->callback_)();
        }
    };

    template <class Callback>
    in_place_stop_callback(in_place_stop_token, Callback) -> in_place_stop_callback<Callback>;
} // namespace clu
