# Header file `meta_algorithm.h`

{% raw %}
``` cpp
#include "type_traits.h"

#include "meta_list.h"

namespace clu
{
    namespace meta
    {
        struct npos_t;

        template <int idx, typename Type>
        struct indexed_type;

        template <typename List>
        struct extract_types;

        template <template <typename ...> typename Templ, typename ... Types>
        struct extract_types<Templ<Types...>;

        template <typename List>
        using extract_types_t = typename extract_types<List>::type;

        template <typename Invocable, typename List>
        using invoke = typename List::template apply<Invocable::apply>;

        template <typename Invocable, typename List>
        using invoke_t = typename List::template apply<Invocable::apply>::type;

        template <typename Invocable, typename List>static constexpr auto invoke_v = List::template  apply<Invocable::apply>::value;

        template <typename ... Types>
        struct enumerate;

        template <typename Target>
        struct contains;

        template <template <typename> typename Pred>
        struct all_of;

        template <template <typename> typename Pred>
        struct any_of;

        template <template <typename> typename Pred>
        struct none_of;

        template <typename Target>
        struct find;

        template <typename Target>
        struct count;

        template <template <typename> typename Pred>
        struct count_if;

        template <template <typename> typename Pred>
        struct filter;

        template <typename ... Types>
        struct is_unique;

        template <typename First, typename ... Rest>
        struct is_unique<First, Rest...>;

        template <typename...Types>inline constexpr bool is_unique_v = is_unique<Types...>::value;

        template <typename ... Types>
        struct unique;

        template <typename First, typename ... Rest>
        struct unique<First, Rest...>;

        template <typename ... Types>
        using unique_t = typename unique<Types...>::type;

        template <typename List1, typename List2>
        using concatenate_t = typename List1::template apply<List2::template concatenate_front>;

        template <typename Set1, typename ... Types2>
        struct 'hidden'<Set1, type_list<Types2...>;

        template <typename Set1, typename Set2>inline constexpr bool set_include_v = set_include<Set1, Set2>::value;

        template <typename Set1, typename Set2>inline constexpr bool set_equal_v = Set1::size == Set2::size && set_include_v<Set1, Set2>;

        template <typename Set1>
        struct 'hidden'<Set1, type_list<>;

        template <typename Set1, typename First, typename ... Rest>
        struct 'hidden'<Set1, type_list<First, Rest...>;

        template <typename Set1, typename Set2>
        using set_intersection_t = typename set_intersection<Set1, Set2>::type;

        template <typename Set2>
        struct 'hidden'<type_list<>, Set2>;

        template <typename First, typename ... Rest, typename Set2>
        struct 'hidden'<type_list<First, Rest...>, Set2>;

        template <typename Set1, typename Set2>
        using set_difference_t = typename set_difference<Set1, Set2>::type;
    }
}
```
{% endraw %}
