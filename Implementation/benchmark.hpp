#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

using Clock = chrono::steady_clock;

inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

struct GoodHasher {
    uint64_t operator()(uint64_t x) const { return splitmix64(x); }
};

struct IdentityHasher {
    uint64_t operator()(uint64_t x) const { return x; }
};

struct Metrics {
    double ns_per_op = 0.0;
    double avg_probes = 0.0;
};

template <typename Hasher>
class SeparateChainingMap {
public:
    explicit SeparateChainingMap(size_t bucket_count, Hasher hasher = {})
        : buckets_(bucket_count), hasher_(hasher) {}

    void insert(uint64_t key, uint64_t value, uint64_t& probes) {
        auto& bucket = buckets_[index_of(key)];
        for (auto& kv : bucket) {
            ++probes;
            if (kv.first == key) {
                kv.second = value;
                return;
            }
        }
        bucket.emplace_back(key, value);
        ++probes;
    }

    const uint64_t* find(uint64_t key, uint64_t& probes) const {
        const auto& bucket = buckets_[index_of(key)];
        for (const auto& kv : bucket) {
            ++probes;
            if (kv.first == key) {
                return &kv.second;
            }
        }
        if (bucket.empty()) {
            ++probes;
        }
        return nullptr;
    }

private:
    vector<vector<pair<uint64_t, uint64_t>>> buckets_;
    Hasher hasher_;

    size_t index_of(uint64_t key) const {
        return static_cast<size_t>(hasher_(key) % buckets_.size());
    }
};

enum class SlotState : uint8_t { Empty, Full };

template <typename Hasher>
class LinearProbingMap {
public:
    explicit LinearProbingMap(size_t capacity, Hasher hasher = {})
        : capacity_(round_up_pow2(capacity)),
          mask_(capacity_ - 1),
          slots_(capacity_),
          hasher_(hasher) {}

    void insert(uint64_t key, uint64_t value, uint64_t& probes) {
        size_t idx = home(key);
        while (true) {
            ++probes;
            Slot& s = slots_[idx];
            if (s.state == SlotState::Empty) {
                s.state = SlotState::Full;
                s.key = key;
                s.value = value;
                return;
            }
            if (s.key == key) {
                s.value = value;
                return;
            }
            idx = (idx + 1) & mask_;
        }
    }

    const uint64_t* find(uint64_t key, uint64_t& probes) const {
        size_t idx = home(key);
        while (true) {
            ++probes;
            const Slot& s = slots_[idx];
            if (s.state == SlotState::Empty) {
                return nullptr;
            }
            if (s.key == key) {
                return &s.value;
            }
            idx = (idx + 1) & mask_;
        }
    }

private:
    struct Slot {
        SlotState state = SlotState::Empty;
        uint64_t key = 0;
        uint64_t value = 0;
    };

    size_t capacity_;
    size_t mask_;
    vector<Slot> slots_;
    Hasher hasher_;

    size_t home(uint64_t key) const {
        return static_cast<size_t>(hasher_(key)) & mask_;
    }

    static size_t round_up_pow2(size_t n) {
        size_t p = 1;
        while (p < n) {
            p <<= 1;
        }
        return p;
    }
};

template <typename Hasher>
class SwissLiteMap {
public:
    explicit SwissLiteMap(size_t capacity, Hasher hasher = {})
        : capacity_(round_up_capacity(capacity)),
          mask_(capacity_ - 1),
          ctrl_(capacity_, kEmpty),
          keys_(capacity_, 0),
          values_(capacity_, 0),
          present_(capacity_, false),
          hasher_(hasher) {}

    void insert(uint64_t key, uint64_t value, uint64_t& probes) {
        uint64_t h = hasher_(key);
        uint8_t tag = h2(h);
        size_t start = h1(h);

        for (size_t group = 0;; ++group) {
            size_t base = (start + group * kGroupSize) & mask_;
            for (size_t i = 0; i < kGroupSize; ++i) {
                size_t pos = (base + i) & mask_;
                ++probes;
                uint8_t c = ctrl_[pos];

                if (c == kEmpty) {
                    ctrl_[pos] = tag;
                    keys_[pos] = key;
                    values_[pos] = value;
                    present_[pos] = true;
                    return;
                }

                if (c == tag && present_[pos] && keys_[pos] == key) {
                    values_[pos] = value;
                    return;
                }
            }
        }
    }

    const uint64_t* find(uint64_t key, uint64_t& probes) const {
        uint64_t h = hasher_(key);
        uint8_t tag = h2(h);
        size_t start = h1(h);

        for (size_t group = 0;; ++group) {
            size_t base = (start + group * kGroupSize) & mask_;
            bool saw_empty = false;

            for (size_t i = 0; i < kGroupSize; ++i) {
                size_t pos = (base + i) & mask_;
                ++probes;
                uint8_t c = ctrl_[pos];

                if (c == kEmpty) {
                    saw_empty = true;
                    continue;
                }

                if (c == tag && present_[pos] && keys_[pos] == key) {
                    return &values_[pos];
                }
            }

            if (saw_empty) {
                return nullptr;
            }
        }
    }

private:
    static constexpr size_t kGroupSize = 16;
    static constexpr uint8_t kEmpty = 0x80;

    size_t capacity_;
    size_t mask_;
    vector<uint8_t> ctrl_;
    vector<uint64_t> keys_;
    vector<uint64_t> values_;
    vector<bool> present_;
    Hasher hasher_;

    size_t h1(uint64_t h) const {
        return static_cast<size_t>(h) & mask_;
    }

    uint8_t h2(uint64_t h) const {
        return static_cast<uint8_t>((h >> 57) & 0x7F);
    }

    static size_t round_up_capacity(size_t n) {
        size_t p = 16;
        while (p < n) {
            p <<= 1;
        }
        return p;
    }
};

template <typename Map>
Metrics time_insert(Map& map, const vector<uint64_t>& keys) {
    uint64_t probes = 0;

    auto t0 = Clock::now();
    for (size_t i = 0; i < keys.size(); ++i) {
        map.insert(keys[i], static_cast<uint64_t>(i), probes);
    }
    auto t1 = Clock::now();

    double ns =
        chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();

    Metrics result;
    result.ns_per_op = ns / keys.size();
    result.avg_probes = static_cast<double>(probes) / keys.size();
    return result;
}

template <typename Map>
Metrics time_find(Map& map, const vector<uint64_t>& keys) {
    uint64_t probes = 0;
    volatile uint64_t sink = 0;

    auto t0 = Clock::now();
    for (uint64_t key : keys) {
        const uint64_t* v = map.find(key, probes);
        if (v != nullptr) {
            sink ^= *v;
        }
    }
    auto t1 = Clock::now();

    (void)sink;

    double ns =
        chrono::duration_cast<chrono::nanoseconds>(t1 - t0).count();

    Metrics result;
    result.ns_per_op = ns / keys.size();
    result.avg_probes = static_cast<double>(probes) / keys.size();
    return result;
}

inline vector<uint64_t> make_random_keys(size_t n, uint64_t seed) {
    mt19937_64 rng(seed);
    vector<uint64_t> v;
    v.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        v.push_back(rng());
    }
    return v;
}

inline vector<uint64_t> make_clustered_keys(size_t n) {
    vector<uint64_t> v;
    v.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        v.push_back(static_cast<uint64_t>(i) << 16);
    }
    return v;
}

inline vector<uint64_t> make_miss_keys(const vector<uint64_t>& keys) {
    vector<uint64_t> misses;
    misses.reserve(keys.size());

    for (uint64_t key : keys) {
        misses.push_back(key ^ 0x9e3779b97f4a7c15ULL);
    }
    return misses;
}

inline size_t bucket_count_for(size_t n, double load_factor) {
    return static_cast<size_t>(ceil(static_cast<double>(n) / load_factor));
}

inline void emit_result(ofstream& csv,
                        const string& map_name,
                        const string& hasher_name,
                        const string& dataset_name,
                        size_t n,
                        double load_factor,
                        const string& operation,
                        const Metrics& m) {
    ostringstream row;
    row << map_name << ','
        << hasher_name << ','
        << dataset_name << ','
        << n << ','
        << load_factor << ','
        << operation << ','
        << fixed << setprecision(3)
        << m.ns_per_op << ','
        << m.avg_probes;

    csv << row.str() << '\n';
    cout << row.str() << '\n';
}

template <typename Hasher>
void run_family(const string& map_name,
                const string& hasher_name,
                const string& dataset_name,
                const vector<uint64_t>& keys,
                double load_factor,
                ofstream& csv) {
    size_t n = keys.size();
    vector<uint64_t> misses = make_miss_keys(keys);

    if (map_name == "SeparateChaining") {
        SeparateChainingMap<Hasher> map(bucket_count_for(n, load_factor));
        Metrics ins = time_insert(map, keys);
        Metrics hit = time_find(map, keys);
        Metrics miss = time_find(map, misses);

        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "insert", ins);
        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "find_hit", hit);
        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "find_miss", miss);
    } else if (map_name == "LinearProbing") {
        LinearProbingMap<Hasher> map(bucket_count_for(n, load_factor));
        Metrics ins = time_insert(map, keys);
        Metrics hit = time_find(map, keys);
        Metrics miss = time_find(map, misses);

        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "insert", ins);
        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "find_hit", hit);
        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "find_miss", miss);
    } else if (map_name == "SwissLite") {
        SwissLiteMap<Hasher> map(bucket_count_for(n, load_factor));
        Metrics ins = time_insert(map, keys);
        Metrics hit = time_find(map, keys);
        Metrics miss = time_find(map, misses);

        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "insert", ins);
        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "find_hit", hit);
        emit_result(csv, map_name, hasher_name, dataset_name, n, load_factor, "find_miss", miss);
    }
}

#endif