# Header file `take.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename T>
    constexpr T take(T& value);
}
```
{% endraw %}

### Function `clu::take`

{% raw %}
``` cpp
template <typename T>
constexpr T take(T& value);
```
{% endraw %}

Reset input value to default and return the original value.

Approximately equivalent to `std::exchange(value, {})`.

-----
