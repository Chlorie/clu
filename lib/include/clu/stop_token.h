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
            requires (!T::stop_possible());
        };
    // clang-format on

    template <typename Token, typename Callback>
        requires stoppable_token_for<Token, std::decay_t<Callback>>
    auto make_stop_callback(Token token, Callback&& callback)
    {
        using callback_type = typename Token::template callback_type<std::decay_t<Callback>>;
        return callback_type(static_cast<Token&&>(token), static_cast<Callback&&>(callback));
    }

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
        ~in_place_stop_source() noexcept;

        [[nodiscard]] in_place_stop_token get_token() noexcept;
        [[nodiscard]] bool stop_possible() const noexcept { return !stop_requested(); }
        [[nodiscard]] bool stop_requested() const noexcept { return requested_.load(std::memory_order::acquire); }
        bool request_stop() noexcept;

    private:
        friend detail::in_place_stop_cb_base;

        struct lock_result
        {
            bool locked = false;
            detail::in_place_stop_cb_base* head = nullptr;
        };

        static constexpr std::uint8_t not_started = 0;
        static constexpr std::uint8_t started = 1;
        static constexpr std::uint8_t completed = 2;

        std::atomic_bool requested_{};
        locked_ptr<detail::in_place_stop_cb_base> callbacks_{};
        std::thread::id requesting_thread_{}; // The thread which requested stop

        bool try_attach(detail::in_place_stop_cb_base* cb) noexcept;
        void detach(detail::in_place_stop_cb_base* cb) noexcept;
        lock_result lock_if_not_requested() noexcept;
    };

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
