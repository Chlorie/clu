#pragma once

#include <type_traits>

namespace clu
{
    template <typename From, typename To>
    struct copy_cvref
    {
    private:
        using removed = std::remove_cvref_t<To>;
        using cv_qual = std::remove_reference_t<From>;
        using cqual = std::conditional_t<std::is_const_v<cv_qual>, std::add_const_t<removed>, removed>;
        using vqual = std::conditional_t<std::is_volatile_v<cv_qual>, std::add_volatile_t<cqual>, cqual>;
        using lref = std::conditional_t<std::is_lvalue_reference_v<From>, std::add_lvalue_reference_t<vqual>, vqual>;
        using rref = std::conditional_t<std::is_rvalue_reference_v<From>, std::add_rvalue_reference_t<lref>, lref>;

    public:
        using type = rref;
    };

    template <typename From, typename To> using copy_cvref_t = typename copy_cvref<From, To>::type;
}
