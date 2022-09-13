#include "clu/random.h"

namespace clu
{
    namespace
    {
        class seed_generator
        {
        public:
            using result_type = std::random_device::result_type;

            template <typename It>
            void generate(It begin, It end)
            {
                for (; begin != end; ++begin)
                    *begin = dev_();
            }

        private:
            std::random_device dev_;
        };
    } // namespace

    std::mt19937& random_engine()
    {
        thread_local std::mt19937 engine = []
        {
            seed_generator seed_gen;
            return std::mt19937(seed_gen);
        }();
        return engine;
    }

    void reseed()
    {
        seed_generator seed_gen;
        random_engine().seed(seed_gen);
    }

    void reseed(const std::mt19937::result_type seed) { random_engine().seed(seed); }
} // namespace clu
