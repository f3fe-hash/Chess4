#include "core/transposition_table.hpp"


constexpr TranspositionTable::Bucket& TranspositionTable::GetBucket(const ZobristHash& key)
{
    const uint64_t bucket_idx = key % BUCKETS;
    return transposition_table[bucket_idx];
}


constexpr TranspositionTable::Bucket TranspositionTable::GetBucket(const ZobristHash key) const
{
    const uint64_t bucket_idx = key % BUCKETS;
    return transposition_table[bucket_idx];
}


static inline float CalculateReplacementScore(const TranspositionTableEntry& entry)
{
    // Calculates a "score" for which entry should be replaced
    const float edepth = static_cast<float>(entry.eval_depth);
    const float mdepth = static_cast<float>(entry.move_depth);

    // Eval depth preference & move depth preference
    constexpr float EVAL_DEPTH_PREF = 2.0;
    constexpr float MOVE_DEPTH_PREF = 1.0;

    const float score = EVAL_DEPTH_PREF * edepth + MOVE_DEPTH_PREF * mdepth;
    return score;
}


void TranspositionTable::Store(
    const ZobristHash& key,
    const TranspositionTableEntry& entry)
{
    Bucket& bucket = GetBucket(key);

    const Entry stored_entry = Entry
    {
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

    if (bucket.n_entries < BUCKET_ENTRIES)
    {
        bucket.entries[bucket.n_entries] = stored_entry;
        ++bucket.n_entries;
        return;
    }

    int replacement_idx = 0;

    for (int entry_idx = 1;
        entry_idx < bucket.n_entries;
        ++entry_idx)
    {
        if (CalculateReplacementScore(bucket.entries[entry_idx].entry) <
            CalculateReplacementScore(bucket.entries[replacement_idx].entry))
        {
            replacement_idx = entry_idx;
        }
    }

    //if (
    //    CalculateReplacementScore(entry) <
    //    CalculateReplacementScore(bucket.entries[replacement_idx].entry)
    //)
    //{
    //    return;
    //}

    bucket.entries[replacement_idx] = stored_entry;
}


TranspositionTableEntry TranspositionTable::Get(
    const ZobristHash& key, bool& found) const
{
    found = false;
    const Bucket& bucket = GetBucket(key);

    for (int entry_idx = 0; entry_idx < bucket.n_entries; entry_idx++)
    {
        const Entry& tt_entry = bucket.entries[entry_idx];
        if (tt_entry.is_valid(key))
        {
            found = true;
            return tt_entry.entry;
        }
    }

    // `found` is already false.
    return {};
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


bool TranspositionTable::Contains(const ZobristHash& key) const
{
    const Bucket& bucket = GetBucket(key);

    for (uint8_t entry_idx = 0; entry_idx < bucket.n_entries; entry_idx++)
    {
        const Entry& tt_entry = bucket.entries[entry_idx];
        if (tt_entry.is_valid(key))
            return true;
    }

    return false;
}


TranspositionTableEntry TranspositionTable::GetEntry(const ZobristHash& key, bool& found) const
{
    return Get(key, found);
}


void TranspositionTable::SetBestMove(
    const ZobristHash& key,
    const Move& move,
    const int& depth)
{
    TranspositionTableEntry entry{};

    bool found = false;
    entry = Get(key, found);
    if (found)
    {
        // Don't replace a deeper entry.
        if (entry.move_depth > depth)
            return;
    }

    entry.best_move = move;
    entry.move_depth = static_cast<uint8_t>(depth);

    Store(key, entry);
}


void TranspositionTable::SetBound(
    const ZobristHash& key,
    const Evaluation& eval,
    const int& depth,
    const TranspositionTableBound& bound)
{
    TranspositionTableEntry entry{};

    bool found = false;
    entry = Get(key, found);
    if (found)
    {
        if (entry.eval_depth > depth)
            return;

        if (entry.eval_depth == depth &&
            entry.bound == TranspositionTableBound::EXACT &&
            bound != TranspositionTableBound::EXACT)
        {
            return;
        }
    }

    entry.eval = eval;
    entry.bound = bound;
    entry.eval_depth = static_cast<uint8_t>(depth);

    Store(key, entry);
}


void TranspositionTable::SetExact(const ZobristHash& key, const Evaluation& exact_eval, const int& depth)
{
    SetBound(key, exact_eval, depth, TranspositionTableBound::EXACT);
}


void TranspositionTable::SetLowerBound(const ZobristHash& key, const Evaluation& lower_eval, const int& depth)
{
    SetBound(key, lower_eval, depth, TranspositionTableBound::LOWER);
}


void TranspositionTable::SetUpperBound(const ZobristHash& key, const Evaluation& upper_eval, const int& depth)
{
    SetBound(key, upper_eval, depth, TranspositionTableBound::UPPER);
}

