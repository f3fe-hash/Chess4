#include "core/transposition_table.hpp"


// If this is updated, please also update the size of TranspositionTable::transposition_table
constexpr int BUCKETS = 100000;


TranspositionTable::TranspositionTable()
{}


void TranspositionTable::_Store(
    const ZobristHash& key,
    const TranspositionTableEntry& entry)
{
    const uint64_t bucket_idx = key % 100000;
    Bucket& bucket = transposition_table[bucket_idx];

    // First look for an existing entry.
    for (Entry& tt_entry : bucket.entries)
    {
        if ((tt_entry.key == key) && tt_entry.valid)
        {
            tt_entry = Entry{entry, key, true};
            return;
        }
    }

    // No existing entry. Replace one.
    // TODO: implement a replacement policy.
    // For now, overwrite existing entry at position 0.
    //
    bucket.entries[0] = Entry{entry, key, true};

}


TranspositionTableEntry TranspositionTable::_Get(
    const ZobristHash& key)
{
    const uint64_t bucket_idx = key % 100000;
    const Bucket& bucket = transposition_table[bucket_idx];

    for (const Entry& tt_entry : bucket.entries)
    {
        if ((tt_entry.key == key) && tt_entry.valid)
            return tt_entry.entry;
    }

    return {};
}


bool TranspositionTable::_Contains(const ZobristHash& key)
{
    const uint64_t bucket_idx = key % 100000;
    const Bucket& bucket = transposition_table[bucket_idx];

    for (const Entry& tt_entry : bucket.entries)
    {
        if ((tt_entry.key == key) && tt_entry.valid)
            return true;
    }

    return false;
}


size_t TranspositionTable::GetNumEntries() const
{
    size_t num_entries = 0;

    for (const Bucket& bucket : transposition_table)
    {
        for (const Entry& entry : bucket.entries)
        {
            if (entry.valid)
                ++num_entries;
        }
    }

    return num_entries;
}


bool TranspositionTable::keyIsStored(const ZobristHash& key)
{
    return _Contains(key);
}


TranspositionTableEntry TranspositionTable::getKey(const ZobristHash& key)
{
    return _Get(key);
}


void TranspositionTable::setBestMove(const ZobristHash& key, const Move& move, const int depth)
{
    // Only store if the depth is higher than the stored depth.
    // Or if the key isn't stored yet.
    if (_Contains(key))
    {
        if (_Get(key).depth <= depth)
            return;
    }
    
    TranspositionTableEntry entry;
    entry.depth = static_cast<uint8_t>(depth);
    entry.best_move = move;
    
    _Store(key, entry);
}


void TranspositionTable::setBound(const ZobristHash& key, const Evaluation exact_eval, const int depth, const TranspositionTableBound bound)
{
    TranspositionTableEntry entry;

    // Only store if the depth is higher than the stored depth.
    // Or if the key isn't stored yet.
    if (_Contains(key))
    {
        entry = _Get(key);
        if (entry.depth <= depth)
            return;
    }

    entry.eval = exact_eval;
    entry.depth = static_cast<uint8_t>(depth);
    entry.bound = bound;

    // Store the entry in the transposition table.
    _Store(key, entry);

}


void TranspositionTable::setExact(const ZobristHash& key, const Evaluation exact_eval, const int depth)
{
    setBound(key, exact_eval, depth, TranspositionTableBound::EXACT);
}


void TranspositionTable::setLowerBound(const ZobristHash& key, const Evaluation lower_eval, const int depth)
{
    setBound(key, lower_eval, depth, TranspositionTableBound::LOWER);
}


void TranspositionTable::setUpperBound(const ZobristHash& key, const Evaluation upper_eval, const int depth)
{
    setBound(key, upper_eval, depth, TranspositionTableBound::UPPER);
}

