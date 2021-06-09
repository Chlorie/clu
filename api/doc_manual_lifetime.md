# Header file `manual_lifetime.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename T>
    class manual_lifetime;

    template <typename T>
    class manual_lifetime<T&>;

    template <typename T>
    class manual_lifetime<T && >;

    template <>
    class manual_lifetime<void>;
}
```
{% endraw %}

### Class `clu::manual_lifetime`

{% raw %}
``` cpp
template <typename T>
class manual_lifetime
{
public:
    manual_lifetime() noexcept;

    ~manual_lifetime() noexcept;

    void destruct() noexcept;
};
```
{% endraw %}

Offers utilities for controlling lifetime of an object manually.

### Constructor `clu::manual_lifetime::manual_lifetime`

{% raw %}
``` cpp
manual_lifetime() noexcept;
```
{% endraw %}

No-op constructor. Space is reserved for the object without starting its lifetime.

-----

### Destructor `clu::manual_lifetime::~manual_lifetime`

{% raw %}
``` cpp
~manual_lifetime() noexcept;
```
{% endraw %}

No-op destructor. Does not destruct potentially living object.

-----

### Function `clu::manual_lifetime::destruct`

{% raw %}
``` cpp
void destruct() noexcept;
```
{% endraw %}

Destruct the controlled object.

-----

-----
