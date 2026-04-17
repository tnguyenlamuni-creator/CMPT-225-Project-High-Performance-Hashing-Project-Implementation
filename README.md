# CMPT-225-Project-High-Performance-Hashing-Project-Implementation
We implemented a C++ microbenchmark with three dictionary variants operating on 64-bit integer keys and values:

• SeparateChaining: a vector of buckets, each bucket implemented as a small dynamic
array of key-value pairs.

• LinearProbing: an open-addressed array using power-of-two capacity and the classic
next-slot probe sequence.

• SwissLite: a simplified SwissTable-inspired layout using grouped probing, one metadata byte per slot, and a 7-bit fingerprint extracted from the full hash. The implementation intentionally focuses on the dictionary core rather than generic C++ container polish. For example, the benchmark uses 64-bit integer keys, so the comparison mostly reflects collision handling and memory layout rather than allocator overhead or expensive string comparisons. SwissLite captures the important ideas from Swiss tables—grouped metadata, small fingerprints, and direct storage—but it does not implement SIMD intrinsics, heterogeneous lookup, industrial-strength deletion logic, or the full growth and tombstone policy used by Abseil. In other words, it is a pedagogical approximation of a production design, not a replacement for Abseil or Folly. Two hash-quality scenarios were benchmarked. The first used random 64-bit keys with a splitmix64-style finalizer, representing the common case of a reasonably well-mixed hash function. The second used clustered keys of the form i << 16 together with an identity hash, intentionally creating a bad low-bit distribution for power-of-two tables. This stress case was included because both the uploaded comparison paper and the hashing literature emphasize that collision behavior depends not only on the collision-resolution scheme but also on the key distribution and hash function.

