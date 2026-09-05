#pragma once

#include <cstdint>
#include <vector>

#include <memory>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <atomic>

#include <unordered_map>

#include "chess.hpp"
#include "core/eval.hpp"
#include "core/transposition_table.hpp"
#include "core/scoring.hpp"
#include "core/multithread.hpp"


struct MoveResult
{
    Move move{};
    Evaluation eval = 0.00;
    uint64_t nodes_searched = 0;
    int64_t mate_in_ply = -2; // < 0 means no mate was found
    int depth = 0;
};


using DurationMs = std::chrono::milliseconds;


class ChessBot
{
    struct SearchParams
    {
        Evaluation alpha;
        Evaluation beta;
        int depth;
        int ply;
        Move move;
        int move_idx;
        bool is_root_search;
        int mate_in;
    };

    ChessBoardEvaluator evaluator;

    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<MoveOrder> move_orderer;

    std::atomic<int64_t> time_limit_ms{0};
    std::chrono::steady_clock::time_point search_start;
    bool time_up;
    std::atomic<bool> stop_requested{false};

    std::shared_ptr<TranspositionTable> transposition_table;

    // MultiThreadManager
    MultiThreadManager<Evaluation, SearchParams> manager;

    uint64_t nodes_searched;

    inline uint64_t GetPositionKey() const
    {
        return board->GetZobristHash();
    }

    Evaluation MainSearch(Evaluation alpha, Evaluation beta, int depth, int ply, int& mate_in);
    Evaluation SearchCore(SearchParams& params);

    int DepthExtension(const Move& move);

public:
    ChessBot(std::shared_ptr<ChessBoard> board);
    ~ChessBot();

    void SetTimeLimit(DurationMs _time_limit);
    DurationMs GetTimeLimit() const
    { return DurationMs(time_limit_ms.load(std::memory_order_relaxed)); }

    inline Evaluation Evaluate()
    { return evaluator.QuiescenceSearch(); }

    inline Evaluation EvaluateRaw()
    { return evaluator.EvaluatePosition(); }

    inline size_t GetTranspositionTableSize() const
    { return transposition_table->GetNumEntries(); }

    inline void Stop()
    { stop_requested.store(true); }

    DurationMs CalculateThinkTime(
        const DurationMs wtime,
        const DurationMs btime,
        const DurationMs orig_wtime,
        const DurationMs orig_btime,
        const DurationMs winc,
        const DurationMs binc
    ) const;

    MoveResult Search(int min_depth, int max_depth);
};
