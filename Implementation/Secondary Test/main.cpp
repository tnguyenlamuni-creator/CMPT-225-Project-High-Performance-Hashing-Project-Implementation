#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>

#include "LinearProbing.h"
#include "RobinHood.h"
#include "SwissLite.h"

template <typename Table>
void run_benchmark(string name, int n) {
    Table t(1 << 16); // Start with 65k capacity
    
    // 1. Benchmark Insertion
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        t.insert(i, i * 10);
    }
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> i_time = end - start;

    // 2. Benchmark Deletion
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < n; ++i) {
        t.remove(i);
    }
    end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> d_time = end - start;

    cout << left << setw(18) << name 
              << " | Ins: " << fixed << setprecision(2) << setw(8) << i_time.count() << "ms"
              << " | Del: " << setw(8) << d_time.count() << "ms" << endl;
}

int main() {
    const int N = 100000; // Testing with 100,000 operations
    
    cout << "--- Hashing Performance Benchmark (N=" << N << ") ---" << endl;
    cout << "Method             | Operation Times" << endl;
    cout << "----------------------------------------------------" << endl;

    run_benchmark<LinearProbingTable<int, int>>("Linear Probing", N);
    run_benchmark<RobinHoodTable<int, int>>("Robin Hood", N);
    run_benchmark<SwissLiteTable<int, int>>("Swiss Lite", N);

    cout << "----------------------------------------------------" << endl;
    return 0;
}