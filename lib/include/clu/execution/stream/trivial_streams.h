#pragma once

#include "base.h"

namespace clu::exec
{
    namespace detail
    {
        namespace opt_snd
        {
            template <typename S, typename R>
            struct ops_t_
            {
                class type;
            };

            template <typename S, typename R>
            using ops_t = typename ops_t_<S, std::decay_t<R>>::type;

            template <typename S, typename R>
            class ops_t_<S, R>::type
            {
            public:
                // clang-format off
                template <typename R2>
                type(S&& snd, R2&& recv):
                    state_(std::in_place_index<0>, copy_elider{[&] { //
                        return connect(static_cast<S&&>(snd), static_cast<R&&>(recv));
                    }}) {}

                template <forwarding<R> R2>
                explicit type(R2&& recv):
                    state_(std::in_place_index<1>, static_cast<R2&&>(recv)) {}
                // clang-format on

            private:
                ops_variant<connect_result_t<S, R>, R> state_;

                friend void tag_invoke(start_t, type& self) noexcept
                {
                    switch (self.state_.index())
                    {
                        case 0: start(get<0>(self.state_)); return;
                        case 1: set_stopped(static_cast<R&&>(get<1>(self.state_))); return;
                        default: unreachable();
                    }
                }
            };

            template <typename S>
            struct snd_t_
            {
                class type;
            };

            template <typename S>
            using snd_t = typename snd_t_<S>::type;

            template <typename S>
            class snd_t_<S>::type
            {
            public:
                explicit type(std::optional<S>&& snd): snd_(std::move(snd)) { snd.reset(); }

            private:
                std::optional<S> snd_;

                template <typename R>
                friend auto tag_invoke(connect_t, type&& self, R&& recv)
                {
                    if (self.snd_)
                        return ops_t<S, R>(static_cast<S&&>(*self.snd_), static_cast<R&&>(recv));
                    else
                        return ops_t<S, R>(static_cast<R&&>(recv));
                }

                // clang-format off
                template <typename Env>
                friend make_completion_signatures<S, Env, completion_signatures<set_stopped_t()>>
                tag_invoke(get_completion_signatures_t, const type&, Env&&) noexcept { return {}; }
                // clang-format on
            };
        } // namespace opt_snd

        template <typename S>
        using optional_sender = opt_snd::snd_t<S>;

        namespace sngl
        {
            template <typename S>
            struct stream_t_
            {
                class type;
            };

            template <typename S>
            using stream_t = typename stream_t_<std::decay_t<S>>::type;

            template <typename S>
            class stream_t_<S>::type
            {
            public:
                // clang-format off
                template <forwarding<S> S2>
                explicit type(S2&& snd): snd_(std::in_place, static_cast<S2>(snd)) {}
                // clang-format on

            private:
                std::optional<S> snd_;
                friend auto tag_invoke(next_t, type& self) { return optional_sender<S>(std::move(self.snd_)); }
            };

            struct single_stream_t
            {
                template <sender S>
                auto operator()(S&& snd) const
                {
                    return stream_t<S>(static_cast<S&&>(snd));
                }
                constexpr auto operator()() const noexcept { return make_piper(*this); }
            };
        } // namespace sngl

        namespace empty_strm
        {
            struct stream_t
            {
                friend auto tag_invoke(next_t, stream_t) noexcept { return just_stopped(); }
            };
            struct empty_stream_t
            {
                constexpr auto operator()() const noexcept { return stream_t{}; }
            };
        } // namespace empty_strm

        namespace rng_strm
        {
            template <typename R>
            struct stream_t_
            {
                class type;
            };

            template <typename R>
            using stream_t = typename stream_t_<R>::type;

            template <typename R>
            class stream_t_<R>::type
            {
            public:
                explicit type(R&& range): range_(static_cast<R&&>(range)) {}

            private:
                R range_;
                std::ranges::iterator_t<R> iter_ = std::ranges::begin(range_);

                friend auto tag_invoke(next_t, type& self)
                {
                    using value_t = std::ranges::range_value_t<R>;
                    using sender_t = decltype(exec::just(std::declval<value_t>()));
                    if (self.iter_ == std::ranges::end(self.range_))
                        return optional_sender<sender_t>({});
                    else
                        return optional_sender<sender_t>(exec::just(*self.iter_++));
                }
            };
        } // namespace rng_strm

        namespace as_strm
        {
            struct as_stream_t
            {
                template <typename T>
                auto operator()(std::initializer_list<T> ilist) const noexcept
                {
                    return rng_strm::stream_t<std::initializer_list<T>>(std::move(ilist));
                }
                template <std::ranges::input_range R>
                auto operator()(R&& range) const
                {
                    return rng_strm::stream_t<R>(static_cast<R&&>(range));
                }
                constexpr auto operator()() const noexcept { return make_piper(*this); }
            };
        } // namespace as_strm
    } // namespace detail

    using detail::sngl::single_stream_t;
    using detail::empty_strm::empty_stream_t;
    using detail::as_strm::as_stream_t;

    inline constexpr single_stream_t single_stream{};
    inline constexpr empty_stream_t empty_stream{};
    inline constexpr as_stream_t as_stream{};
} // namespace clu::exec
