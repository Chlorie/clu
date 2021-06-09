# Header file `type_list.h`

{% raw %}
``` cpp
namespace clu
{
    namespace meta
    {
        template <typename ... Ts>
        struct type_list;

        template <typename T>
        struct type_tag;

        template <int I, typename T>
        struct indexed_type;

        template <typename L>
        struct list_size;

        template <typename ... Ts>
        struct list_size<type_list<Ts...>;

        template <typename L>inline constexpr size_t list_size_v

        template <typename S>
        struct to_type_list;

        template <typename S>
        using to_type_list_t = typename to_type_list<S>::type;

        template <typename L>
        struct to_integer_sequence;

        template <typename L>
        using to_integer_sequence_t = typename to_integer_sequence<L>::type;

        template <typename L>
        struct extract_list;

        template <template <typename ...> typename Templ, typename ... Ts>
        struct extract_list<Templ<Ts...>;

        template <typename T>
        using extract_list_t = typename extract_list<T>::type;

        template <typename L, template <typename ...> typename Templ>
        struct apply;

        template <typename ... Ts, template <typename ...> typename Templ>
        struct apply<type_list<Ts...>, Templ>;

        template <typename L, template <typename ...> typename Templ>
        using apply_t = typename apply<L, Templ>::type;

        template <typename L>
        struct front;

        template <typename T, typename ... Rest>
        struct front<type_list<T, Rest...>;

        template <typename L>
        using front_t = typename front<L>::type;

        template <typename L>
        struct tail;

        template <typename T, typename ... Rest>
        struct tail<type_list<T, Rest...>;

        template <typename L>
        using tail_t = typename tail<L>::type;

        template <typename L, typename M>
        struct concatenate;

        template <typename ... Ts, typename ... Us>
        struct concatenate<type_list<Ts...>, type_list<Us...>;

        template <typename L, typename M>
        using concatenate_t = typename concatenate<L, M>::type;

        template <typename L, typename T>
        struct find;

        template <typename T>
        struct find<type_list<>, T>;

        template <typename First, typename ... Rest, typename T>
        struct find<type_list<First, Rest...>, T>;

        template <typename L, typename T>inline constexpr size_t find_v

        template <typename L, typename T>
        struct contains;

        template <typename ... Ts, typename T>
        struct contains<type_list<Ts...>, T>;

        template <typename L, typename T>inline constexpr bool contains_v = contains<L, T>::value;

        template <typename L, int N>
        struct at;

        template <typename T, typename ... Rest>
        struct at<type_list<T, Rest...>, 0>;

        template <typename T, typename ... Rest, int N>
        struct at<type_list<T, Rest...>, N>;

        template <typename L, int N>
        using at_t = typename at<L, N>::type;
    }
}
```
{% endraw %}
