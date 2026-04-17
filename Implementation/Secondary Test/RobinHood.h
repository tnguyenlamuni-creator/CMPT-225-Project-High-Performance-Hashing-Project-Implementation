#ifndef ROBIN_HOOD_H
#define ROBIN_HOOD_H

#include <vector>
#include <algorithm>

using namespace std;

template <typename K, typename V>
class RobinHoodTable {
    struct Entry {
        K key;
        V val;
        int psl = -1; // Probe Sequence Length (-1 = empty)
    };

    vector<Entry> table;
    size_t capacity;
    size_t count = 0;

public:
    RobinHoodTable(size_t cap = 16) : capacity(cap), table(cap) {}

    void insert(K key, V val) {
        if (count > capacity * 0.9) rehash();

        Entry incoming = {key, val, 0};
        size_t idx = hash<K>{}(key) & (capacity - 1);

        while (true) {
            if (table[idx].psl == -1) {
                table[idx] = incoming;
                count++;
                return;
            }

            if (table[idx].key == incoming.key) {
                table[idx].val = incoming.val;
                return;
            }

            // Swap if newcomer is "poorer" (further from home)
            if (incoming.psl > table[idx].psl) {
                swap(incoming, table[idx]);
            }

            idx = (idx + 1) & (capacity - 1);
            incoming.psl++;
        }
    }

    bool remove(K key) {
        size_t idx = hash<K>{}(key) & (capacity - 1);
        while (table[idx].psl != -1) {
            if (table[idx].key == key) {
                // Backward-shift deletion to avoid tombstones
                size_t next = (idx + 1) & (capacity - 1);
                while (table[next].psl > 0) {
                    table[idx] = table[next];
                    table[idx].psl--;
                    idx = next;
                    next = (idx + 1) & (capacity - 1);
                }
                table[idx].psl = -1;
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
        table.assign(capacity, {K(), V(), -1});
        count = 0;
        for (auto& e : old_table) {
            if (e.psl != -1) insert(e.key, e.val);
        }
    }
};

#endif