#pragma once

#include "stream_basic.h"
#include "composed.h"

namespace clu::exec
{
    inline struct for_each_t
    {
        template <stream S, typename F>
        auto operator()(S&& strm, F&& func) const
        {
            return reduce(static_cast<S&&>(strm), unit{},
                [f = static_cast<F&&>(func)]<typename... Ts>(unit, Ts&&... args) mutable
                {
                    (void)f(static_cast<Ts&&>(args)...);
                    return unit{};
                });
        }

        template <typename F>
        auto operator()(F&& func) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr for_each{};

    inline struct upon_each_t
    {
        template <stream S, typename F>
        auto operator()(S&& strm, F&& func) const
        {
            return adapt_next(static_cast<S&&>(strm),
                [f = static_cast<F&&>(func)]<typename Snd>(Snd&& snd) mutable
                { return then(static_cast<Snd&&>(snd), f); });
        }

        template <typename F>
        auto operator()(F&& func) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr upon_each{};

    inline struct let_each_t
    {
        template <stream S, typename F>
        auto operator()(S&& strm, F&& func) const
        {
            return adapt_next(static_cast<S&&>(strm),
                [f = static_cast<F&&>(func)]<typename Snd>(Snd&& snd) mutable
                { return let_value(static_cast<Snd&&>(snd), std::ref(f)); });
        }

        template <typename F>
        auto operator()(F&& func) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr let_each{};

    inline struct stream_size_t
    {
        template <stream S>
        auto operator()(S&& strm) const
        {
            return reduce(strm, 0_uz, //
                [](const std::size_t partial, auto&&...) noexcept { return just(partial + 1); });
        }
        constexpr auto operator()() const noexcept { return make_piper(*this); }
    } constexpr stream_size{};

    inline struct count_if_t
    {
        template <stream S, typename Pred>
        auto operator()(S&& strm, Pred&& pred) const
        {
            return reduce(static_cast<S&&>(strm), 0_uz,
                [p = static_cast<Pred&&>(pred)]<typename... Args>(const std::size_t partial, Args&&... args)
                {
                    return then(p(static_cast<Args&&>(args)...), //
                        [=](const bool cond) noexcept { return partial + cond; });
                });
        }

        template <typename Pred>
        constexpr auto operator()(Pred&& pred) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Pred&&>(pred)));
        }
    } constexpr count_if{};

    inline struct count_t
    {
        template <stream S, typename T>
        auto operator()(S&& strm, T&& value) const
        {
            return reduce(static_cast<S&&>(strm), 0_uz, //
                [v = static_cast<T&&>(value)](const std::size_t partial, const auto& current)
                { return partial + (v == current); });
        }

        template <typename T>
        constexpr auto operator()(T&& value) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<T&&>(value)));
        }
    } constexpr count{};

    inline struct first_t
    {
        template <stream S>
        auto operator()(S&& strm) const
        {
            return finally(next(strm), cleanup(strm));
        }
        constexpr auto operator()() const noexcept { return make_piper(*this); }
    } constexpr first{};

    inline struct each_on_t
    {
        template <stream Strm, scheduler Schd>
        auto operator()(Strm&& strm, Schd&& schd) const
        {
            return adapt_next(static_cast<Strm&&>(strm),
                [s = static_cast<Schd&&>(schd)]<typename Snd>(Snd&& snd) mutable
                { return on(s, static_cast<Snd&&>(snd)); });
        }

        template <scheduler Schd>
        constexpr auto operator()(Schd&& schd) const
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Schd&&>(schd)));
        }
    } constexpr each_on{};
} // namespace clu::exec
