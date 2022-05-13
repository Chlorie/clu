#pragma once

#include "execution_traits.h"

namespace clu::exec
{
    namespace detail
    {
        namespace inl_schd
        {
            template <typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename R>
            class ops_t_<R>::type
            {
            public:
                // clang-format off
                template <forwarding<R> R2>
                explicit type(R2&& recv): recv_(static_cast<R2&&>(recv)) {}
                CLU_IMMOVABLE_TYPE(type);
                // clang-format on

            private:
                R recv_;

                friend void tag_invoke(start_t, type& self) noexcept { exec::set_value(static_cast<R&&>(self.recv_)); }
            };

            template <typename R>
            using ops_t = typename ops_t_<std::remove_cvref_t<R>>::type;

            struct CLU_API schd_t
            {
                struct CLU_API snd_t
                {
                    template <typename R>
                    friend ops_t<R> tag_invoke(connect_t, snd_t, R&& recv)
                    {
                        return ops_t<R>(static_cast<R&&>(recv));
                    }

                    // clang-format off
                    friend schd_t tag_invoke(
                        get_completion_scheduler_t<set_value_t>, snd_t) noexcept { return {}; }
                    friend completion_signatures<set_value_t()> tag_invoke(
                        get_completion_signatures_t, snd_t, auto&&) noexcept { return {}; }
                    // clang-format on
                };

                constexpr friend bool operator==(schd_t, schd_t) noexcept = default;
                friend snd_t tag_invoke(schedule_t, schd_t) noexcept { return {}; }
            };
        } // namespace inl_schd

        namespace trmp_schd
        {
            template <typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename R>
            using ops_t = typename ops_t_<std::remove_cvref_t<R>>::type;

            class ops_base_t
            {
            public:
                explicit ops_base_t(const std::size_t depth) noexcept: depth_(depth) {}
                CLU_IMMOVABLE_TYPE(ops_base_t);
                virtual void execute() noexcept = 0;
                ops_base_t* next() const noexcept { return next_; }

            protected:
                std::size_t depth_;
                ops_base_t* next_ = nullptr;

                virtual ~ops_base_t() noexcept = default;
                void add_operation() noexcept;
            };

            template <typename R>
            class ops_t_<R>::type final : public ops_base_t
            {
            public:
                // clang-format off
                template <typename R2>
                type(R2&& recv, const std::size_t depth):
                    ops_base_t(depth), recv_(static_cast<R2&&>(recv)) {}
                // clang-format on

                void execute() noexcept override
                {
                    [[maybe_unused]] auto token = get_stop_token(get_env(recv_));
                    if constexpr (unstoppable_token<decltype(token)>)
                        set_value(static_cast<R&&>(recv_));
                    else if (token.stop_requested())
                        set_stopped(static_cast<R&&>(recv_));
                    else
                        set_value(static_cast<R&&>(recv_));
                }

            private:
                R recv_;

                friend void tag_invoke(start_t, type& self) noexcept { self.add_operation(); }
            };

            class CLU_API schd_t
            {
            public:
                // clang-format off
                constexpr explicit schd_t(const std::size_t max_recursion_depth = 16) noexcept:
                    depth_(max_recursion_depth) {}
                // clang-format on

            private:
                class CLU_API snd_t
                {
                public:
                    constexpr explicit snd_t(const std::size_t depth) noexcept: depth_(depth) {}

                private:
                    std::size_t depth_;

                    template <typename R>
                    friend auto tag_invoke(connect_t, const snd_t self, R&& recv)
                    {
                        return ops_t<R>(static_cast<R&&>(recv), self.depth_);
                    }

                    // clang-format off
                    friend schd_t tag_invoke(get_completion_scheduler_t<set_value_t>, //
                        const snd_t self) noexcept { return schd_t(self.depth_); }
                    friend completion_signatures<set_value_t(), set_stopped_t()> tag_invoke(
                        get_completion_signatures_t, snd_t, auto&&) noexcept { return {}; }
                    // clang-format on
                };

                std::size_t depth_;

                friend snd_t tag_invoke(schedule_t, const schd_t self) noexcept { return snd_t(self.depth_); }
                constexpr friend bool operator==(schd_t, schd_t) noexcept { return true; }
            };
        } // namespace trmp_schd
    } // namespace detail

    using inline_scheduler = detail::inl_schd::schd_t;
    using trampoline_scheduler = detail::trmp_schd::schd_t;
} // namespace clu::exec
