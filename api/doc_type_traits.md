# Header file `type_traits.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename Type>
    struct type_tag_t;

    template <typename Type>inline constexpr type_tag_t<Type>type_tag{};

    template <auto val>
    struct value_tag_t;

    template <auto val>inline constexpr value_tag_t<val>value_tag{};

    template <bool value, typename True, typename False>
    using conditional_t = typename detail::conditional_impl<value>::template type<True, False>;

    template <typename...Ts>inline constexpr bool dependent_false = false;

    template <typename From, typename To>
    struct copy_cvref;

    template <typename From, typename To>
    using copy_cvref_t = typename copy_cvref<From, To>::type;

    template <typename ... Ts>
    struct all_same;

    template <>
    struct all_same;

    template <typename T>
    struct all_same<T>;

    template <typename T, typename ... Rest>
    struct all_same<T, Rest...>;

    template <typename...Ts>inline constexpr bool all_same_v = all_same<Ts...>::value;

    template <typename T>inline constexpr bool no_cvref_v = std::is_same_v<T, std::remove_cvref_t<T>>;

    template <typename T>
    struct no_cvref;
}
```
{% endraw %}
