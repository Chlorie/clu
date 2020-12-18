#include <iostream>
#include <vector>
#include <clu/type_erasure.h>

struct MeowableModel
{
    template <typename Base>
    struct interface : Base
    {
        void meow() { clu::vdispatch<0>(*this); }
        void meow() const { clu::vdispatch<1>(*this); }
        void meow(const int x) const { clu::vdispatch<2>(*this, x); }
    };

    template <typename T>
    using members = clu::meta::type_list<
        clu::member_sig<void (T::*)(), &T::meow>,
        clu::member_sig<void (T::*)() const, &T::meow>,
        clu::member_sig<void (T::*)(int) const, &T::meow>
    >;
};

struct BlurringShadow // https://github.com/BlurringShadow
{
    void meow() { std::cout << "Meow!\n"; }
    void meow() const { std::cout << "Const meow!\n"; }
    void meow(int) const { std::cout << "int meow!\n"; }
};

struct CatGirl
{
    void meow() { std::cout << "Nya!\n"; }
    void meow() const { std::cout << "Const nya!\n"; }
    void meow(int) const { std::cout << "int nya!\n"; }
};

int main()
{
    namespace tep = clu::type_erasure_policy;
    using Meowable = clu::type_erased<MeowableModel, tep::copyable>;
    std::vector<Meowable> meowables;
    meowables.emplace_back(BlurringShadow{});
    meowables.emplace_back(BlurringShadow{});
    meowables.emplace_back(CatGirl{});
    for (auto& m : meowables) m.meow();
    for (const auto& m : std::as_const(meowables)) m.meow();
    for (auto& m : meowables) m.meow(42);
}
