#pragma once

#include <unordered_map>
#include <cstdint>

#ifdef DEBUG
#include <iomanip>
#endif

#include "chess.hpp"
#include "core/scoring.hpp"


#ifdef RELEASE
// We don't really want to replace many
// entries in release mode, as it is expensive.
inline const int BUCKET_ENTRIES = 32;

// More buckets in release mode
inline const int BUCKETS = 16384;
#else
// For debug, use smaller bucket sizes.
inline const int BUCKET_ENTRIES = 16;

inline const int BUCKETS = 4096;
#endif


enum class TranspositionTableBound
{
    // Default
    NONE,

    // We have found the EXACT evaluation (at this depth)
    EXACT,

    // We have found a LOWER bound for the evaluation (at this depth)
    LOWER,

    // We have found an UPPER bound for the evaluation (at this depth)
    UPPER,
};


#ifdef DEBUG

#define DEBUG_TT_STATS
#define DEBUG_TT_CAPACITY

struct TTDebugData
{
#ifdef DEBUG_TT_STATS
    // Write bound statistics
    std::uint64_t tt_write_bound_exact = 0;
    std::uint64_t tt_write_bound_lower = 0;
    std::uint64_t tt_write_bound_upper = 0;

    std::vector<TranspositionTableBound> read_bounds{};

    // Read / write statistics
    std::uint64_t tt_writes = 0;
    std::uint64_t tt_reads = 0;
    std::uint64_t tt_valid_reads = 0;
#endif

#ifdef DEBUG_TT_CAPACITY
    // Capacity
    std::uint64_t overwrites = 0;
    std::uint64_t occupancy = 0;
    std::uint64_t capacity = 0;
    std::uint64_t capacityBytes = 0;
#endif
};

#endif


struct TranspositionTableEntry
{
    Evaluation eval = 0.00;
    TranspositionTableBound bound = TranspositionTableBound::NONE;
    Move best_move{};
    uint8_t eval_depth = 0;
    uint8_t move_depth = 0;

    TranspositionTableEntry() = default;
    TranspositionTableEntry(const TranspositionTableEntry&) = default; // Copy
    TranspositionTableEntry& operator = (const TranspositionTableEntry&) = default;
};


class TranspositionTable
{
    struct Entry
    {
        TranspositionTableEntry entry{};
        ZobristHash key{};
        bool valid_ = false;

        Entry() = default;

        Entry(
            const TranspositionTableEntry& entry,
            ZobristHash key,
            bool valid)
            : entry(entry),
            key(key),
            valid_(valid)
        {
        }

        inline bool is_valid(const ZobristHash& key) const
        {
            return valid_ && key == this->key;
        }

        inline bool is_valid() const
        {
            return valid_;
        }
    };

    struct Bucket
    {
        Entry entries[BUCKET_ENTRIES];
        int n_entries = 0;
    };

    // Zobrist hash -> Transposition entry data
    Bucket transposition_table[BUCKETS];

    constexpr Bucket& GetBucket(const ZobristHash& key);
    constexpr Bucket GetBucket(const ZobristHash key) const;

    void Store(const ZobristHash& key, const TranspositionTableEntry& entry);
    TranspositionTableEntry Get(const ZobristHash& key, bool& found) const;

    void SetBound(const ZobristHash& key, const Evaluation& exact_eval, const int& depth, const TranspositionTableBound& bound);

public:
    TranspositionTable();
    ~TranspositionTable() = default;
    TranspositionTable(const TranspositionTable&) = default; // Copy

    std::size_t GetNumEntries() const;

    bool Contains(const ZobristHash& key) const;

    TranspositionTableEntry GetEntry(const ZobristHash& key, bool& found) const;
    
    void SetBestMove(const ZobristHash& key, const Move& move, const int& depth);

    void SetExact       (const ZobristHash& key, const Evaluation& exact_eval, const int& depth);
    void SetLowerBound  (const ZobristHash& key, const Evaluation& lower_eval, const int& depth);
    void SetUpperBound  (const ZobristHash& key, const Evaluation& upper_eval, const int& depth);
};

#ifdef DEBUG
void PrintTTDebug();
void ClearTTDebug();
#endif
