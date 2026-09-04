#include "core/transposition_table.hpp"


// If this is updated, please also update the size of TranspositionTable::transposition_table
constexpr int BUCKETS = 65535;


void TranspositionTable::_Store(
    const ZobristHash& key,
    const TranspositionTableEntry& entry)
{
    const uint32_t bucket_idx = key % BUCKETS;
    Bucket& bucket = transposition_table[bucket_idx];

    const Entry stored_entry{
        entry,
        key,
        true
    };

    // --------------------------------------------------------
    // Existing entry?
    // --------------------------------------------------------

    for (int entry_idx = 0;
         entry_idx < bucket.n_entries;
         ++entry_idx)
    {
        Entry& tt_entry = bucket.entries[entry_idx];

        if (tt_entry.is_valid(key))
        {
            // Replace existing entry.
            //
            // IMPORTANT:
            // n_entries must NOT change.
            tt_entry = stored_entry;
            return;
        }
    }

    // --------------------------------------------------------
    // Empty slot?
    // --------------------------------------------------------

    if (bucket.n_entries < 8)
    {
        bucket.entries[bucket.n_entries] = stored_entry;
        ++bucket.n_entries;
        return;
    }

    // Prefer retaining deeper searches when a bucket collides.
    int replacement_idx = 0;
    for (int entry_idx = 1; entry_idx < bucket.n_entries; ++entry_idx)
    {
        if (bucket.entries[entry_idx].entry.depth <
            bucket.entries[replacement_idx].entry.depth)
        {
            replacement_idx = entry_idx;
        }
    }

    bucket.entries[replacement_idx] = stored_entry;
}


TranspositionTableEntry TranspositionTable::_Get(
    const ZobristHash& key)
{
    const uint32_t bucket_idx = key % BUCKETS;
    const Bucket& bucket = transposition_table[bucket_idx];

    for (int entry_idx = 0; entry_idx < bucket.n_entries; entry_idx++)
    {
        const Entry& tt_entry = bucket.entries[entry_idx];
        if (tt_entry.is_valid(key))
            return tt_entry.entry;
    }

    return {};
}


bool TranspositionTable::_Contains(const ZobristHash& key)
{
    const uint32_t bucket_idx = key % BUCKETS;
    const Bucket& bucket = transposition_table[bucket_idx];

    for (int entry_idx = 0; entry_idx < bucket.n_entries; entry_idx++)
    {
        const Entry& tt_entry = bucket.entries[entry_idx];
        if (tt_entry.is_valid(key))
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
            if (entry.valid_)
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


void TranspositionTable::setBestMove(
    const ZobristHash& key,
    const Move& move,
    const int depth)
{
    TranspositionTableEntry entry{};

    if (_Contains(key))
    {
        entry = _Get(key);

        // Don't replace a deeper entry.
        if (entry.depth > depth)
            return;
    }

    entry.best_move = move;

    // Only update depth if this is a deeper entry.
    if (entry.depth < depth)
        entry.depth = static_cast<uint8_t>(depth);

    _Store(key, entry);
}


void TranspositionTable::setBound(
    const ZobristHash& key,
    const Evaluation eval,
    const int depth,
    const TranspositionTableBound bound)
{
    TranspositionTableEntry entry{};

    if (_Contains(key))
    {
        entry = _Get(key);

        // Don't replace a deeper entry with a shallower one.
        if (entry.depth > depth)
            return;
    }

    entry.eval = eval;
    entry.bound = bound;
    entry.depth = static_cast<uint8_t>(depth);

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

