#pragma once

#include <coroutine>
#include <concepts>
#include <ranges>

namespace clu
{
    template <
        std::ranges::input_range R,
        typename Alloc = std::allocator<std::byte>>
    struct elements_of
    {
    public:
        constexpr explicit elements_of(R&& range) noexcept
            requires std::is_default_constructible_v<Alloc>: range_(static_cast<R&&>(range)) {}

        constexpr explicit elements_of(R&& range, Alloc alloc) noexcept:
            range_(static_cast<R&&>(range)), alloc_(alloc) {}

        constexpr elements_of(const elements_of&) = delete;
        constexpr elements_of(elements_of&&) = default;
        constexpr elements_of& operator=(const elements_of&) = delete;
        constexpr elements_of& operator=(elements_of&&) = delete;

        [[nodiscard]] constexpr R&& range() noexcept { return static_cast<R&&>(range_); }
        [[nodiscard]] constexpr const Alloc& allocator() const noexcept { return alloc_; }

    private:
        R&& range_;
        [[no_unique_address]] Alloc alloc_;
    };

    template <typename R> elements_of(R) -> elements_of<R>;
    template <typename R, typename A> elements_of(R, A&&) -> elements_of<R, A>;

    namespace detail { }

    template <
        typename Ref,
        std::common_reference_with<Ref> Value = std::remove_cvref_t<Ref>,
        typename Alloc = void>
    class generator { };
}

template <typename Ref, typename Value, typename Alloc>
inline constexpr bool std::ranges::enable_view<clu::generator<Ref, Value, Alloc>> = true;
