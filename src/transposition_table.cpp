#include "transposition_table.hpp"


bool TranspositionTable::keyIsStored(const ZobristHash& key)
{
    return transposition_table.contains(key);
}


TranspositionTableEntry TranspositionTable::getKey(const ZobristHash& key)
{
    return transposition_table.at(key);
}


void TranspositionTable::setBestMove(const ZobristHash& key, const Move& move, int depth)
{
    // Only store if the depth is higher than the stored depth.
    // Or if the key isn't stored yet.
    if (keyIsStored(key))
        if (transposition_table.at(key).depth <= depth)
            return;
    
    TranspositionTableEntry entry;
    entry.depth = static_cast<uint8_t>(depth);
    entry.best_move = move;
    
    transposition_table.insert_or_assign(
        key,
        entry
    );
}


void TranspositionTable::setExact(const ZobristHash& key, Evaluation exact_eval, int depth)
{
    TranspositionTableEntry entry;

    // Only store if the depth is higher than the stored depth.
    // Or if the key isn't stored yet.
    if (keyIsStored(key))
    {
        entry = transposition_table.at(key);
        if (entry.depth <= depth)
            return;
    }

    entry.eval = exact_eval;
    entry.depth = static_cast<uint8_t>(depth);
    entry.bound = TranspositionTableBound::EXACT;

    // Store the entry in the transposition table.
    transposition_table.insert_or_assign(
        key,
        entry
    );
}


void TranspositionTable::setLowerBound(const ZobristHash& key, Evaluation lower_eval, int depth)
{
    TranspositionTableEntry entry;

    // Only store if the depth is higher than the stored depth.
    // Or if the key isn't stored yet.
    if (keyIsStored(key))
    {
        entry = transposition_table.at(key);
        if (entry.depth <= depth)
            return;
    }
    
    entry.eval = lower_eval;
    entry.depth = static_cast<uint8_t>(depth);
    entry.bound = TranspositionTableBound::LOWER;

    // Store the entry in the transposition table.
    transposition_table.insert_or_assign(
        key,
        entry
    );
}


void TranspositionTable::setUpperBound(const ZobristHash& key, Evaluation upper_eval, int depth)
{
    TranspositionTableEntry entry;

    // Only store if the depth is higher than the stored depth.
    // Or if the key isn't stored yet.
    if (keyIsStored(key))
    {
        entry = transposition_table.at(key);
        if (entry.depth <= depth)
            return;
    }

    entry.eval = upper_eval;
    entry.depth = static_cast<uint8_t>(depth);
    entry.bound = TranspositionTableBound::UPPER;

    // Store the entry in the transposition table.
    transposition_table.insert_or_assign(
        key,
        entry
    );
}

