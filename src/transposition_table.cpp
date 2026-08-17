#include "transposition_table.hpp"


// If this is updated, please also update the size of TranspositionTable::transposition_table
constexpr int BUCKETS = 100000;


TranspositionTable::TranspositionTable()
{
    for (size_t i = 0; i < BUCKETS; i++)
    {
        // With 100,000 buckets, it is VERY uncommon to find multiple entries in the same bucket.
        transposition_table[i].reserve(4);
    }
}


void TranspositionTable::_Store(const ZobristHash& key, const TranspositionTableEntry& entry_)
{
    uint64_t bucket_idx = key % BUCKETS;

    const Entry entry = (Entry){
        entry_, key
    };

    for (size_t entry_idx = 0; entry_idx < GetBucketSize(bucket_idx); entry_idx++)
    {
        // Use a reference so we can directly set it instead of re-addressing the TT.
        Entry& tt_entry = transposition_table[bucket_idx][entry_idx];

        if (tt_entry.key == key)
        {
            // We have it. Just set it to the correct value.
            tt_entry = entry;
            break;
        }
    }

    // We don't have that entry. Add it to the end.
    transposition_table[bucket_idx].push_back(entry);
}


TranspositionTableEntry TranspositionTable::_Get(const ZobristHash& key)
{
    uint64_t bucket_idx = key % BUCKETS;

    TranspositionTableEntry entry;
    for (size_t entry_idx = 0; entry_idx < GetBucketSize(bucket_idx); entry_idx++)
    {
        Entry& tt_entry = transposition_table[bucket_idx][entry_idx];
        if (tt_entry.key == key)
        {
            // Found it!
            entry = tt_entry.entry;
            break;
        }
    }
    
    return entry;
}


bool TranspositionTable::_Contains(const ZobristHash& key)
{
    uint64_t bucket_idx = key % BUCKETS;

    for (size_t entry_idx = 0; entry_idx < GetBucketSize(bucket_idx); entry_idx++)
    {
        Entry entry = transposition_table[bucket_idx][entry_idx];
        if (entry.key == key)
            return true;
    }

    return false;
}


size_t TranspositionTable::GetNumEntries()
{
    size_t num_entries = 0;
    for (size_t bucket_idx = 0; bucket_idx < BUCKETS; bucket_idx++)
    {
        num_entries += GetBucketSize(bucket_idx);
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

