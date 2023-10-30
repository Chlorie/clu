#include <iostream>
#include <clu/random.h>

int main()
{
    std::vector population{1, 4, 2, 3, 9, 7, 6, 0};
    std::vector weights{0.7, 0.4, 0.6, 1.2, 0.3, 0.6, 2.0, 0.1};
    std::vector<double> cumulative_weights;
    std::inclusive_scan(weights.begin(), weights.end(), std::back_inserter(cumulative_weights), std::plus<>{}, 0.);

    constexpr std::size_t n = 4;
    std::vector<int> results(n);

    clu::cumulative_weighted_sample(population, cumulative_weights, results.begin(), n);
    std::cout << "Sample with replacements:\n";
    for (const int e : results)
        std::cout << e << ' ';
    std::cout << '\n';

    clu::weighted_sample_no_replacements(population, weights, results.begin(), n);
    std::cout << "Sample without replacements:\n";
    for (const int e : results)
        std::cout << e << ' ';
    std::cout << '\n';
}
