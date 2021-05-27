#include <clu/meta_list.h>
#include <clu/experimental/meta/algorithm.h>

namespace meta = clu::meta;

constexpr size_t accumulate_0_to_99()
{
    using indseq = std::make_index_sequence<100>;
    using tlist = meta::to_type_list_t<indseq>;
    using result_t = meta::reduce_t<tlist, meta::plus, meta::integral_constant<0>>;
    return result_t::value;
}
static_assert(accumulate_0_to_99() == 4950);

template <size_t N>
constexpr size_t factorial = meta::reduce_t<
    meta::transform_t<
        meta::to_type_list_t<std::make_index_sequence<N>>,
        meta::bind_front<meta::plus, meta::integral_constant<1>>::call
    >,
    meta::multiplies,
    meta::integral_constant<1>
>::value;

static_assert(factorial<3> == 6);
static_assert(factorial<10> == 3628800);

static_assert(meta::count_v<meta::type_list<int, double, float, int>, int> == 2);

int main() {} // Nothing to run, this code is correct if it compiles.
