# Header file `outcome.h`

{% raw %}
``` cpp
namespace clu
{
    struct exceptional_outcome_t;

    struct void_tag_t;

    class bad_outcome_access final;

    template <typename T>
    class outcome final;

    template <>
    class outcome<void> final;

    template <typename T>
    class outcome<T&> final;

    template <typename T>
    class outcome<T && > final;

    outcome(void_tag_t)->outcome<void>;
}
```
{% endraw %}
