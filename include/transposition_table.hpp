#pragma once

#include <unordered_map>

#include "chess.hpp"


using Evaluation = float;


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
    uint8_t depth;

    TranspositionTableBound bound;

    Move best_move;

    TranspositionTableEntry() = default;

    TranspositionTableEntry(const TranspositionTableEntry& other)
    {
        *this = other;
    }

    void operator = (const TranspositionTableEntry& other)
    {
        eval = other.eval;
        depth = other.depth;
        bound = other.bound;
        best_move = other.best_move;
    }
};


class TranspositionTable
{
    struct Entry
    {
        TranspositionTableEntry entry;
        ZobristHash key;

        Entry() = default;

        Entry(const Entry& other)
        {
            *this = other;
        }

        void operator = (const Entry& other)
        {
            entry = other.entry;
            key = other.key;
        }
    };

    // Zobrist hash -> Transposition entry data
    //std::unordered_map<ZobristHash, Entry> transposition_table;
    std::vector<Entry> transposition_table[100000];

    // Return the size of a bucket.
    inline size_t GetBucketSize(const uint64_t& bucket)
    { return static_cast<size_t> (transposition_table[bucket].size()); } 

    void _Store(const ZobristHash& key, const TranspositionTableEntry& entry);
    TranspositionTableEntry _Get(const ZobristHash& key);
    bool _Contains(const ZobristHash& key);

    void setBound(const ZobristHash& key, const Evaluation exact_eval, const int depth, const TranspositionTableBound bound);

public:
    TranspositionTable();
    ~TranspositionTable() {}

    size_t GetNumEntries();

    bool keyIsStored(const ZobristHash& key);

    TranspositionTableEntry getKey(const ZobristHash& key);
    
    void setBestMove(const ZobristHash& key, const Move& move, const int depth);

    void setExact       (const ZobristHash& key, const Evaluation exact_eval, const int depth);
    void setLowerBound  (const ZobristHash& key, const Evaluation lower_eval, const int depth);
    void setUpperBound  (const ZobristHash& key, const Evaluation upper_eval, const int depth);
};

