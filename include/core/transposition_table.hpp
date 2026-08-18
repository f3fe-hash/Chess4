#pragma once

#include <unordered_map>

#include "chess.hpp"
#include "core/scoring.hpp"


enum class TranspositionTableBound
{
    // We have found the EXACT evaluation (at this depth)
    EXACT,

    // We have found a LOWER bound for the evaluation (at this depth)
    LOWER,

    // We have found an UPPER bound for the evaluation (at this depth)
    UPPER,
};


struct TranspositionTableEntry
{
    Evaluation eval;
    TranspositionTableBound bound;
    Move best_move;
    uint8_t depth;

    TranspositionTableEntry() = default;
    TranspositionTableEntry(const TranspositionTableEntry&) = default;
    TranspositionTableEntry& operator=(const TranspositionTableEntry&) = default;
};


class TranspositionTable
{
    struct Entry
    {
        TranspositionTableEntry entry{};
        ZobristHash key{};
        bool valid;

        Entry() = default;

        Entry(const TranspositionTableEntry& entry, ZobristHash key, bool valid)
            : entry(entry), key(key), valid(valid)
        {
        }
    };

    struct Bucket
    {
        Entry entries[8];
    };

    // Zobrist hash -> Transposition entry data
    Bucket transposition_table[65535];

    void _Store(const ZobristHash& key, const TranspositionTableEntry& entry);
    TranspositionTableEntry _Get(const ZobristHash& key);
    bool _Contains(const ZobristHash& key);

    void setBound(const ZobristHash& key, const Evaluation exact_eval, const int depth, const TranspositionTableBound bound);

public:
    TranspositionTable();
    ~TranspositionTable() {}

    size_t GetNumEntries() const;

    bool keyIsStored(const ZobristHash& key);

    TranspositionTableEntry getKey(const ZobristHash& key);
    
    void setBestMove(const ZobristHash& key, const Move& move, const int depth);

    void setExact       (const ZobristHash& key, const Evaluation exact_eval, const int depth);
    void setLowerBound  (const ZobristHash& key, const Evaluation lower_eval, const int depth);
    void setUpperBound  (const ZobristHash& key, const Evaluation upper_eval, const int depth);
};

