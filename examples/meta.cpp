#include <clu/meta_algorithm.h>

namespace meta = clu::meta;

static_assert(meta::count_lv<clu::type_list<int, double, float, int>, int> == 2);

template <std::size_t N>
using size_t_ = std::integral_constant<std::size_t, N>;
template <std::size_t N>
using index_type_seq = decltype([]<std::size_t... I>(std::index_sequence<I...>) { //
    return clu::type_list<size_t_<I>...>{};
}(std::make_index_sequence<N>{}));

template <typename T>
using plus_one = size_t_<T::value + 1>;
template <typename T, typename U>
using multiplies = size_t_<T::value * U::value>;
template <std::size_t N>
using factorial = meta::foldl_l< //
    meta::transform_l<index_type_seq<N>, meta::quote1<plus_one>>, //
    meta::quote2<multiplies>, size_t_<1>>;

static_assert(factorial<0>::value == 1);
static_assert(factorial<10>::value == 3628800);

int main() {} // Nothing to run, this code is correct if it compiles.
