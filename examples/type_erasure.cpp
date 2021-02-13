#include <iostream>
#include <string>
#include <clu/type_erasure.h>

struct MeowableModel
{
    // `interface` should be a member type template inheriting from
    // its template parameter, it should also "implement" the members
    // using clu::vdispatch calls with appropriate indices
    template <typename Base>
    struct interface : Base
    {
        void meow() { clu::vdispatch<0>(*this); }
        void meow() const { clu::vdispatch<1>(*this); }
        void meow(const int x) const { clu::vdispatch<2>(*this, x); }
    };

    // `members` should be a member type alias template of a value list
    // holding all of the interface functions and their respective
    // member function pointer signatures
    // Non-overload functions do not need static_cast
    template <typename T>
    using members = clu::meta::value_list<
        static_cast<void (T::*)()>(&T::meow),
        static_cast<void (T::*)() const>(&T::meow),
        static_cast<void (T::*)(int) const>(&T::meow)
    >;
};

namespace te = clu::te;

struct CatGirl
{
    void meow() { std::cout << "nya~\n"; }
    void meow() const { std::cout << "consuto nya~\n"; }
    void meow(const int x) const { std::cout << x << " nya~\n"; }
};

struct ImmovableCat
{
    ImmovableCat() = default;
    ImmovableCat(const ImmovableCat&) = delete;
    ImmovableCat& operator=(const ImmovableCat&) = delete;

    void meow() { std::cout << "meow\n"; }
    void meow() const { std::cout << "const meow\n"; }
    void meow(const int x) const { std::cout << x << " meow\n"; }
};

struct LargeCat
{
    std::string name;

    void meow() { std::cout << name << " meows\n"; }
    void meow() const { std::cout << name << " meows constantly\n"; }
    void meow(const int x) const { std::cout << name << " meowed " << x << " time(s)\n"; }
};

// Overload new and delete operator for monitoring heap allocation
void* operator new(const std::size_t size)
{
    std::printf("Allocated, size = %zu\n", size);
    void* ptr = std::malloc(size);
    return ptr ? ptr : throw std::bad_alloc();
}

void operator delete(void* ptr) noexcept
{
    std::puts("Freed some memory");
    std::free(ptr);
}

void basic_usage()
{
    // When used with no policies, a `type_erased` object manages the
    // underlying object like a `unique_ptr`, in that it always allocates
    // memory on the heap. The difference is that a `type_erased` with
    // default policies is non-null and has value semantics, that you
    // call the members with dots instead of arrows. It is also not
    // copyable, but always moveable, even if the type used to construct
    // the `type_erased` object is non-moveable.
    using Meowable = clu::type_erased<MeowableModel>;
    std::cout << "Basic usage:\n";
    {
        // Meowable cat; // Not default constructible
        // Implicit conversion from a type that implements the interface
        Meowable cat = CatGirl{};
        // Using the interface
        cat.meow();
        std::as_const(cat).meow();
        cat.meow(42);

        auto& ref = cat.as<CatGirl>(); // Get the concrete type back as ref
        ref.meow();
        auto* ptr = cat.as_ptr<CatGirl>(); // Or as a pointer
        ptr->meow(100);
        auto* no_large_cat = cat.as_ptr<LargeCat>(); // nullptr when types mismatch
        std::cout << "Large cat? " << (no_large_cat != nullptr) << '\n';
        try
        {
            // Throws std::bad_cast when getting as reference and types mismatch
            auto& no_large_cat_ref = cat.as<LargeCat>();
            no_large_cat_ref.meow(); // Should not reach this
        }
        catch (const std::bad_cast& e)
        {
            std::cout << e.what() << '\n';
        }
    }
    std::cout << '\n';
    // Emplacement
    {
        // Meowable cat = ImmovableCat{}; // Not moveable
        Meowable cat(std::in_place_type<ImmovableCat>); // Could construct in situ
        cat.meow();
        Meowable moved_cat = std::move(cat); // But the `type_erased` can be moved
        moved_cat.meow();
        moved_cat.emplace<CatGirl>(); // Emplace with another type of cat
        moved_cat.meow();
    }
    std::cout << "\n";
}

void nullable_and_copyable()
{
    std::cout << "Nullable & copyable:\n";
    {
        using NullableCat = clu::type_erased<MeowableModel, te::nullable>;
        NullableCat cat; // Now it can be default constructed, holding the null state
        std::cout << "cat is empty: " << cat.empty() << '\n';
        // cat.meow(); // No.
        cat = CatGirl{};
        std::cout << "cat is empty: " << cat.empty() << '\n';
        if (cat) cat.meow(42); // Conversion to bool
        if (cat != nullptr) cat.meow(42); // Comparison with nullptr
    }
    std::cout << '\n';
    {
        using CopyCat = clu::type_erased<MeowableModel, te::copyable>;
        CopyCat cat = CatGirl{};
        CopyCat copy_cat = cat; // Copyable cat
        cat.meow();
        copy_cat.meow();
    }
    std::cout << "\n";
}

void stack_buffer()
{
    std::cout << "Stack buffers:\n";
    {
        // Use small buffer optimization: smaller cats will be stored in the stack
        // buffer without heap allocation. To be storable on the stack, the stored
        // type should be nothrow move constructible, of a size smaller than the
        // buffer, and not overaligned (its alignment should be less than that of
        // std::max_align_t).
        using SmallCatOpt = clu::type_erased<MeowableModel, te::small_buffer<8>>;
        SmallCatOpt cat = CatGirl{}; // No allocation
        cat.meow();
        cat.emplace<ImmovableCat>(); // Size ok, but not move c'tible, so this allocates
        cat.meow();
        cat = LargeCat("Meowy"); // Fat cat, doesn't fit in the small buffer
        cat.meow();
    }
    std::cout << '\n';
    {
        // `stack_only` is like `small_buffer`, except when the type cannot be stored
        // in the stack buffer it just refuses to compile.
        using RealSmallCats = clu::type_erased<MeowableModel, te::stack_only<8>>;
        RealSmallCats cat = CatGirl{}; // No allocation
        cat.meow();
        // Following types are not suitable for the stack buffer
        // cat.emplace<ImmovableCat>();
        // cat.emplace<LargeCat>("Meowy");
    }
}

int main()
{
    std::cout << std::boolalpha;
    basic_usage();
    nullable_and_copyable();
    stack_buffer();
}
