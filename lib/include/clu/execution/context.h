#pragma once

#include "execution_traits.h"

namespace clu::exec
{
    namespace detail::ctx
    {
        template <typename Ctx>
        class ops_base;

        struct add_operation_t
        {
            template <typename Ctx>
                requires(tag_invocable<add_operation_t, Ctx&, ops_base<Ctx>&>)
            void operator()(Ctx& context, ops_base<Ctx>& ops) const { clu::tag_invoke(*this, context, ops); }
        };

        template <typename Ctx>
        class ops_base
        {
        public:
            virtual void execute() = 0;
            ops_base* next = nullptr;

        protected:
            Ctx* ctx_ = nullptr;
            explicit ops_base(Ctx* ctx) noexcept: ctx_(ctx) {}
            ~ops_base() noexcept = default;
            void enqueue() { ctx_->add(this); }
        };

        template <typename Ctx>
        struct snd_t_
        {
            class type;
        };

        template <typename Ctx>
        using snd_t = typename snd_t_<Ctx>::type;

        template <typename Ctx, typename R>
        struct ops_t_
        {
            class type;
        };

        template <typename Ctx, typename R>
        using ops_t = typename ops_t_<Ctx, std::remove_cvref_t<R>>::type;

        template <typename Ctx, typename R>
        class ops_t_<Ctx, R>::type final : public ops_base<Ctx>
        {
        public:
            // clang-format off
            template <forwarding<R> R2>
            type(Ctx* ctx, R2&& recv) noexcept(std::is_nothrow_constructible_v<R, R2>):
                ops_base<Ctx>(ctx), recv_(static_cast<R2&&>(recv)) {}
            ~type() noexcept = default;
            // clang-format on

            void execute() override
            {
                if (exec::get_stop_token(recv_).stop_requested())
                    exec::set_stopped(std::move(recv_));
                else
                    exec::set_value(std::move(recv_));
            }

        private:
            R recv_;

            friend void tag_invoke(start_t, type& state) noexcept
            {
                try
                {
                    add_operation_t{}(*state.ctx_, state);
                }
                catch (...)
                {
                    exec::set_error(std::move(state.recv_), std::current_exception());
                }
            }
        };

        template <typename Ctx>
        struct schd_t_
        {
            class type;
        };

        template <typename Ctx>
        using schd_t = typename schd_t_<Ctx>::type;

        template <typename Ctx>
        class snd_t_<Ctx>::type
        {
        public:
            explicit type(Ctx* ctx): ctx_(ctx) {}

        private:
            Ctx* ctx_ = nullptr;

            template <typename R>
            friend auto tag_invoke(connect_t, const type& snd, R&& recv) noexcept(
                std::is_nothrow_constructible_v<std::remove_cvref_t<R>, R>)
            {
                return ops_t<Ctx, R>(snd.ctx_, static_cast<R&&>(recv));
            }

            // clang-format off
            template <recvs::completion_cpo Cpo>
            friend auto tag_invoke(get_completion_scheduler_t<Cpo>, //
                const type& snd) noexcept { return schd_t<Ctx>(snd.ctx_); }

            constexpr friend completion_signatures<set_value_t(), set_error_t(std::exception_ptr), set_stopped_t()>
            tag_invoke(get_completion_signatures_t, const type&, auto&&) noexcept { return {}; }
            // clang-format on
        };

        template <typename Ctx>
        class schd_t_<Ctx>::type
        {
        public:
            explicit type(Ctx* ctx) noexcept: ctx_(ctx) {}
            friend bool operator==(type, type) noexcept = default;

        private:
            Ctx* ctx_;
            friend snd_t<Ctx> tag_invoke(schedule_t, const type& self) noexcept { return snd_t<Ctx>(self.ctx_); }
        };

        // clang-format off
        template <typename Ctx>
        concept addable_context = callable<add_operation_t, Ctx&, ops_base<Ctx>&>;
        // clang-format on
    } // namespace detail::ctx

    template <detail::ctx::addable_context Ctx>
    using context_scheduler = typename detail::ctx::schd_t_<Ctx>::type;

    template <typename Ctx>
    using context_operation_state_base = detail::ctx::ops_base<Ctx>;

    using detail::ctx::add_operation_t;
    inline constexpr add_operation_t add_operation{};
} // namespace clu::exec
