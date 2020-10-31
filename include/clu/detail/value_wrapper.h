#pragma once

#include <variant>

namespace clu::detail
{
    template <typename T> concept lr_reference = std::is_reference_v<T>;

    template <typename T>
    class value_wrapper final
    {
    private:
        std::variant<std::monostate, T, std::exception_ptr> data_;

        template <typename S>
        static auto&& get_impl(S&& self)
        {
            if (const std::exception_ptr* eptrptr = std::get_if<2>(&self.data_))
                std::rethrow_exception(*eptrptr);
            return std::get<1>(std::forward<S>(self).data_);
        }

    public:
        template <typename... Ts> void emplace(Ts&&... args) { data_.template emplace<1>(std::forward<Ts>(args)...); }
        void capture_exception() { data_.template emplace<2>(std::current_exception()); }

        bool completed() const noexcept { return data_.index() != 0; }

        T& get() & { return get_impl(*this); }
        const T& get() const & { return get_impl(*this); }
        T&& get() && { return get_impl(std::move(*this)); }
        const T& get() const && { return get_impl(std::move(*this)); }
    };

    template <lr_reference T>
    class value_wrapper<T> final
    {
    private:
        using remove_ref = std::remove_reference_t<T>;
        std::variant<std::monostate, remove_ref*, std::exception_ptr> data_;

        static remove_ref* addressof(remove_ref& lref) { return std::addressof(lref); }

    public:
        void emplace(T ref) { data_.template emplace<1>(addressof(ref)); }
        void capture_exception() { data_.template emplace<2>(std::current_exception()); }

        bool completed() const noexcept { return data_.index() != 0; }

        T get() const
        {
            if (const std::exception_ptr* eptrptr = std::get_if<2>(&data_))
                std::rethrow_exception(*eptrptr);
            return static_cast<T>(*std::get<1>(data_));
        }
    };
}
