#pragma once

namespace clu
{
    // @formatter:off
    /// Overload set type. Useful for `std::visit`.
    template <typename... Fs> struct overload : Fs... { using Fs::operator()...; };
    template <typename... Fs> overload(Fs ...) -> overload<Fs...>;
    // @formatter:on
}
