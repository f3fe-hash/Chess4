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
};


class TranspositionTable
{
    // Zobrist hash -> Transposition entry data
    std::unordered_map<ZobristHash, TranspositionTableEntry> transposition_table;

public:
    TranspositionTable() {}
    ~TranspositionTable() {}

    bool keyIsStored(const ZobristHash& key);

    TranspositionTableEntry getKey(const ZobristHash& key);
    
    void setBestMove(const ZobristHash& key, const Move& move, int depth);

    void setExact(const ZobristHash& key, Evaluation exact_eval, int depth);
    void setLowerBound(const ZobristHash& key, Evaluation lower_eval, int depth);
    void setUpperBound(const ZobristHash& key, Evaluation upper_eval, int depth);
};
