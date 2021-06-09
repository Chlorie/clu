# Header file `algorithm.h`

{% raw %}
``` cpp
#include "type_list.h"

#include "functional.h"

namespace clu
{
    namespace meta
    {
        template <typename List, template <typename,typename> typename BinaryFunc, typename Init>
        struct reduce;

        template <template <typename,typename> typename BinaryFunc, typename Init>
        struct reduce<type_list<>, BinaryFunc, Init>;

        template <typename First, typename ... Rest, template <typename,typename> typename BinaryFunc, typename Init>
        struct reduce<type_list<First, Rest...>, BinaryFunc, Init>;

        template <typename List, template <typename,typename> typename BinaryFunc, typename Init>
        using reduce_t = typename reduce<List, BinaryFunc, Init>::type;

        template <typename List, template <typename> typename UnaryFunc>
        struct transform;

        template <typename ... Ts, template <typename> typename UnaryFunc>
        struct transform<type_list<Ts...>, UnaryFunc>;

        template <typename List, template <typename> typename UnaryFunc>
        using transform_t = typename transform<List, UnaryFunc>::type;

        template <typename List, template <typename> typename Pred>
        struct all_of;

        template <typename ... Ts, template <typename> typename Pred>
        struct all_of<type_list<Ts...>, Pred>;

        template <typename List, template <typename>typename Pred>inline constexpr bool all_of_v = all_of<List, Pred>::value;

        template <typename List, template <typename> typename Pred>
        struct any_of;

        template <typename ... Ts, template <typename> typename Pred>
        struct any_of<type_list<Ts...>, Pred>;

        template <typename List, template <typename>typename Pred>inline constexpr bool any_of_v = any_of<List, Pred>::value;

        template <typename List, template <typename> typename Pred>
        struct none_of;

        template <typename ... Ts, template <typename> typename Pred>
        struct none_of<type_list<Ts...>, Pred>;

        template <typename List, template <typename>typename Pred>inline constexpr bool none_of_v = none_of<List, Pred>::value;

        template <typename List, typename T>
        struct count;

        template <typename ... Ts, typename T>
        struct count<type_list<Ts...>, T>;

        template <typename List, typename T>inline constexpr size_t count_v = count<List, T>::value;

        template <typename List, template <typename> typename Pred>
        struct count_if;

        template <typename ... Ts, template <typename> typename Pred>
        struct count_if<type_list<Ts...>, Pred>;

        template <typename List, template <typename>typename Pred>inline constexpr size_t count_if_v = count_if<List, Pred>::value;
    }
}
```
{% endraw %}
