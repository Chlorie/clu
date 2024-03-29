#include <iostream>
#include <format>
#include <clu/box.h>
#include <clu/memory_resource.h>

class verbose_memory_resource final : public std::pmr::memory_resource
{
public:
    explicit verbose_memory_resource(
        std::string name, memory_resource* base = std::pmr::get_default_resource()) noexcept: //
        name_(std::move(name)),
        base_(base)
    {
    }

protected:
    void* do_allocate(const std::size_t bytes, const std::size_t align) override
    {
        void* ptr = base_->allocate(bytes, align);
        std::cout << std::format("[{}] +{} ({}B, alignof {})\n", name_, ptr, bytes, align);
        return ptr;
    }

    void do_deallocate(void* ptr, const std::size_t bytes, const std::size_t align) override
    {
        std::cout << std::format("[{}] -{} ({}B, alignof {})\n", name_, ptr, bytes, align);
        base_->deallocate(ptr, bytes, align);
    }

    bool do_is_equal(const memory_resource& other) const noexcept override
    {
        if (const auto* pother = reinterpret_cast<const verbose_memory_resource*>(&other))
            return pother->base_->is_equal(*base_);
        return false;
    }

private:
    std::string name_;
    memory_resource* base_;
};

struct S
{
    virtual ~S() = default;
    int things = 42;
};

struct T final : S
{
    int more_things = 42;
};

int main() // NOLINT
{
    verbose_memory_resource vmr("vmr");
    clu::non_propagating_polymorphic_allocator<> alloc(&vmr);
    std::cout << "ints_box:\n";
    auto ints_box = clu::allocate_box<int[]>(alloc, 16);
    ints_box.swap(ints_box);
    std::cout << "int_box:\n";
    auto int_box = clu::allocate_box<int>(alloc, 42);
    std::cout << "s_box:\n";
    auto s_box = clu::allocate_box<T>(alloc);
    std::cout << "destructors:\n";
}
