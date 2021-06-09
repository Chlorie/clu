# Header file `functional.h`

{% raw %}
``` cpp
namespace clu
{
    namespace meta
    {
        template <auto Value>
        using integral_constant = std::integral_constant<decltype(Value), Value>;

        template <typename T>
        struct is_integral_constant;

        template <typename T, T V>
        struct is_integral_constant<std::integral_constant<T, V>;

        template <typename T>inline constexpr bool is_integral_constant_v = is_integral_constant<T>::value;

        template <template <typename ...> typename Func, typename ... Ts>
        struct bind_front;

        template <template <typename ...> typename Func, typename ... Ts>
        struct bind_back;

        template <typename T, typename U>
        struct plus;

        template <typename T, typename U>
        struct minus;

        template <typename T, typename U>
        struct multiplies;

        template <typename T, typename U>
        struct divides;

        template <typename T, typename U>
        struct modulus;

        template <typename T>
        struct negate;

        template <typename T, typename U>
        struct equal_to;

        template <typename T, typename U>
        struct not_equal_to;

        template <typename T, typename U>
        struct less;

        template <typename T, typename U>
        struct greater;

        template <typename T, typename U>
        struct less_equal;

        template <typename T, typename U>
        struct greater_equal;

        template <typename T, typename U>
        struct logical_and;

        template <typename T, typename U>
        struct logical_or;

        template <typename T>
        struct logical_not;

        template <typename T, typename U>
        struct bit_and;

        template <typename T, typename U>
        struct bit_or;

        template <typename T, typename U>
        struct bit_xor;

        template <typename T>
        struct bit_not;

        template <template <typename> typename UnaryFunc>
        struct not_fn;
    }
}
```
{% endraw %}
