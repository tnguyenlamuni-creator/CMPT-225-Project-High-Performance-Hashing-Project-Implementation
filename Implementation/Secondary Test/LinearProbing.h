#ifndef LINEAR_PROBING_H
#define LINEAR_PROBING_H

#include <vector>
#include <optional>

using namespace std;

template <typename K, typename V>
class LinearProbingTable {
    struct Entry {
        K key;
        V val;
        bool occupied = false;
        bool deleted = false;
    };

    vector<Entry> table;
    size_t count = 0;
    size_t capacity;

public:
    LinearProbingTable(size_t cap = 16) : capacity(cap), table(cap) {}

    void insert(K key, V val) {
        if (count > capacity * 0.7) rehash();
        
        size_t idx = hash<K>{}(key) & (capacity - 1);
        while (table[idx].occupied) {
            if (!table[idx].deleted && table[idx].key == key) break;
            if (table[idx].deleted) break; // Reuse deleted slots
            idx = (idx + 1) & (capacity - 1);
        }
        if (!table[idx].occupied || table[idx].deleted) count++;
        table[idx] = {key, val, true, false};
    }

    bool remove(K key) {
        size_t idx = hash<K>{}(key) & (capacity - 1);
        while (table[idx].occupied) {
            if (!table[idx].deleted && table[idx].key == key) {
                table[idx].deleted = true;
                count--;
                return true;
            }
            idx = (idx + 1) & (capacity - 1);
        }
        return false;
    }

private:
    void rehash() {
        auto old_table = move(table);
        capacity *= 2;
        table.assign(capacity, {K(), V(), false, false});
        count = 0;
        for (auto& e : old_table) {
            if (e.occupied && !e.deleted) insert(e.key, e.val);
        }
    }
};

#endif