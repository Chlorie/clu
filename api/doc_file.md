# Header file `file.h`

{% raw %}
``` cpp
namespace clu
{
    template <typename T>
    std::vector<T> read_all_bytes(std::filesystem::path const& path);
}
```
{% endraw %}

### Function `clu::read_all_bytes`

{% raw %}
``` cpp
template <typename T>
std::vector<T> read_all_bytes(std::filesystem::path const& path);
```
{% endraw %}

Read contents of a binary file into a `std::vector`.

*Return values:* `std::vector` with the file content.

#### Template parameters

  - `T` - Value type of the result vector, must be a trivial type.

#### Parameters

  - `path` - Path to the file to read.

-----
