# Header file `overload.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename ... Fs>
    struct overload;
}
```
{% endraw %}

### Struct `clu::overload`

{% raw %}
``` cpp
template <typename ... Fs>
struct overload
: Fs
{
    using Fs::operator();
};
```
{% endraw %}

Overload set type. Useful for `std::visit`.

-----
