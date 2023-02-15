#pragma once

#include "stream_basic.h"
#include "composed.h"

namespace clu::exec
{
    inline struct for_each_t
    {
        template <stream S, typename F>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, F&& func) 
        {
            return reduce(static_cast<S&&>(strm), unit{},
                [f = static_cast<F&&>(func)]<typename... Ts>(unit, Ts&&... args) mutable
                {
                    (void)f(static_cast<Ts&&>(args)...);
                    return unit{};
                });
        }

        template <typename F>
        CLU_STATIC_CALL_OPERATOR(auto)(F&& func) 
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr for_each{};

    inline struct upon_each_t
    {
        template <stream S, typename F>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, F&& func) 
        {
            return adapt_next(static_cast<S&&>(strm),
                [f = static_cast<F&&>(func)]<typename Snd>(Snd&& snd) mutable
                { return then(static_cast<Snd&&>(snd), f); });
        }

        template <typename F>
        CLU_STATIC_CALL_OPERATOR(auto)(F&& func) 
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr upon_each{};

    inline struct let_each_t
    {
        template <stream S, typename F>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, F&& func) 
        {
            return adapt_next(static_cast<S&&>(strm),
                [f = static_cast<F&&>(func)]<typename Snd>(Snd&& snd) mutable
                { return let_value(static_cast<Snd&&>(snd), std::ref(f)); });
        }

        template <typename F>
        CLU_STATIC_CALL_OPERATOR(auto)(F&& func) 
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<F&&>(func)));
        }
    } constexpr let_each{};

    inline struct stream_size_t
    {
        template <stream S>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm) 
        {
            return reduce(strm, 0_uz, //
                [](const std::size_t partial, auto&&...) noexcept { return just(partial + 1); });
        }
        constexpr CLU_STATIC_CALL_OPERATOR(auto)()  noexcept { return make_piper(*this); }
    } constexpr stream_size{};

    inline struct count_if_t
    {
        template <stream S, typename Pred>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, Pred&& pred) 
        {
            return reduce(static_cast<S&&>(strm), 0_uz,
                [p = static_cast<Pred&&>(pred)]<typename... Args>(const std::size_t partial, Args&&... args)
                {
                    return then(p(static_cast<Args&&>(args)...), //
                        [=](const bool cond) noexcept { return partial + cond; });
                });
        }

        template <typename Pred>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(Pred&& pred) 
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Pred&&>(pred)));
        }
    } constexpr count_if{};

    inline struct count_t
    {
        template <stream S, typename T>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm, T&& value) 
        {
            return reduce(static_cast<S&&>(strm), 0_uz, //
                [v = static_cast<T&&>(value)](const std::size_t partial, const auto& current)
                { return partial + (v == current); });
        }

        template <typename T>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(T&& value) 
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<T&&>(value)));
        }
    } constexpr count{};

    inline struct first_t
    {
        template <stream S>
        CLU_STATIC_CALL_OPERATOR(auto)(S&& strm) 
        {
            return finally(next(strm), cleanup(strm));
        }
        constexpr CLU_STATIC_CALL_OPERATOR(auto)()  noexcept { return make_piper(*this); }
    } constexpr first{};

    inline struct each_on_t
    {
        template <stream Strm, scheduler Schd>
        CLU_STATIC_CALL_OPERATOR(auto)(Strm&& strm, Schd&& schd) 
        {
            return adapt_next(static_cast<Strm&&>(strm),
                [s = static_cast<Schd&&>(schd)]<typename Snd>(Snd&& snd) mutable
                { return on(s, static_cast<Snd&&>(snd)); });
        }

        template <scheduler Schd>
        constexpr CLU_STATIC_CALL_OPERATOR(auto)(Schd&& schd) 
        {
            return clu::make_piper(clu::bind_back(*this, static_cast<Schd&&>(schd)));
        }
    } constexpr each_on{};
} // namespace clu::exec
