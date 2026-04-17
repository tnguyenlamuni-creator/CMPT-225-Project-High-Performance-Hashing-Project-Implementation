#include <fstream>
#include <iostream>
#include <vector>

#include "benchmark.hpp"

using namespace std;

int main() {
    ofstream csv("benchmark_results.csv");
    if (!csv) {
        cerr << "Failed to open benchmark_results.csv for writing.\n";
        return 1;
    }

    csv << "map,hasher,dataset,n,load_factor,operation,ns_per_op,avg_probes\n";
    csv.flush();

    const vector<size_t> sizes = {2000, 5000};
    const vector<double> load_factors = {0.50, 0.80};

    cout << "Starting benchmark...\n";

    for (size_t n : sizes) {
        vector<uint64_t> random_keys = make_random_keys(n, 123456789ULL + n);
        vector<uint64_t> clustered_keys = make_clustered_keys(n);

        for (double lf : load_factors) {
            cout << "Running n = " << n
                 << ", load factor = " << lf << '\n';

            run_family<GoodHasher>(
                "SeparateChaining", "good_mix", "random", random_keys, lf, csv);
            run_family<GoodHasher>(
                "LinearProbing", "good_mix", "random", random_keys, lf, csv);
            run_family<GoodHasher>(
                "SwissLite", "good_mix", "random", random_keys, lf, csv);

            run_family<IdentityHasher>(
                "SeparateChaining", "identity", "clustered", clustered_keys, lf, csv);
            run_family<IdentityHasher>(
                "LinearProbing", "identity", "clustered", clustered_keys, lf, csv);
            run_family<IdentityHasher>(
                "SwissLite", "identity", "clustered", clustered_keys, lf, csv);

            csv.flush();
        }
    }

    csv.close();
    cout << "Wrote benchmark_results.csv\n";
    return 0;
}