#ifndef SWISS_LITE_H
#define SWISS_LITE_H

#include <vector>
#include <cstdint>

using namespace std;

template <typename K, typename V>
class SwissLiteTable {
    enum Ctrl : int8_t { kEmpty = -128, kDeleted = -2 };
    vector<int8_t> ctrl;
    vector<pair<K, V>> data;
    size_t capacity;
    size_t count = 0;

public:
    SwissLiteTable(size_t cap = 16) : capacity(cap), ctrl(cap, kEmpty), data(cap) {}

    void insert(K key, V val) {
        if (count > capacity * 0.8) rehash();

        uint64_t h = hash<K>{}(key);
        int8_t tag = static_cast<int8_t>(h & 0x7F);
        size_t idx = h & (capacity - 1);

        while (ctrl[idx] != kEmpty) {
            if (ctrl[idx] == tag && data[idx].first == key) {
                data[idx].second = val;
                return;
            }
            idx = (idx + 1) & (capacity - 1);
        }
        ctrl[idx] = tag;
        data[idx] = {key, val};
        count++;
    }

    bool remove(K key) {
        uint64_t h = hash<K>{}(key);
        int8_t tag = static_cast<int8_t>(h & 0x7F);
        size_t idx = h & (capacity - 1);

        while (ctrl[idx] != kEmpty) {
            if (ctrl[idx] == tag && data[idx].first == key) {
                ctrl[idx] = kDeleted;
                count--;
                return true;
            }
            idx = (idx + 1) & (capacity - 1);
        }
        return false;
    }

private:
    void rehash() {
        auto old_ctrl = move(ctrl);
        auto old_data = move(data);
        capacity *= 2;
        ctrl.assign(capacity, kEmpty);
        data.resize(capacity);
        count = 0;
        for (size_t i = 0; i < old_ctrl.size(); ++i) {
            if (old_ctrl[i] >= 0) insert(old_data[i].first, old_data[i].second);
        }
    }
};

#endif