# Header file `hash.h`

{% raw %}
``` cpp
namespace clu
{
    constexpr uint32_t const fnv_prime_32 = 0x01000193u;

    constexpr uint64_t const fnv_prime_64 = 0x00000100000001b3ull;

    constexpr uint32_t const fnv_offset_basis_32 = 0x811c9dc5u;

    constexpr uint64_t const fnv_offset_basis_64 = 0xcbf29ce484222325ull;

    inline namespace literals
    {
    }
}
```
{% endraw %}

### Variable `clu::fnv_prime_32`

{% raw %}
``` cpp
constexpr uint32_t const fnv_prime_32 = 0x01000193u;
```
{% endraw %}

32-bit FNV prime

-----

### Variable `clu::fnv_prime_64`

{% raw %}
``` cpp
constexpr uint64_t const fnv_prime_64 = 0x00000100000001b3ull;
```
{% endraw %}

64-bit FNV prime

-----

### Variable `clu::fnv_offset_basis_32`

{% raw %}
``` cpp
constexpr uint32_t const fnv_offset_basis_32 = 0x811c9dc5u;
```
{% endraw %}

32-bit FNV offset basis

-----

### Variable `clu::fnv_offset_basis_64`

{% raw %}
``` cpp
constexpr uint64_t const fnv_offset_basis_64 = 0xcbf29ce484222325ull;
```
{% endraw %}

64-bit FNV offset basis

-----
