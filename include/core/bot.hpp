#pragma once

#include <cstdint>
#include <vector>

#include <memory>
#include <chrono>
#include <algorithm>
#include <atomic>
#include <cmath>

#include <unordered_map>

// DEBUG
#ifdef DEBUG
#include <iostream>

// Tracks how many times LMR was incorrect, and a re-search was necessary.
#define DEBUG_LMR_RESEARCH

struct BotDebugData
{
    // DEBUG_LMR_SEARCH
    std::uint64_t lmr_research_count = 0;
    std::uint64_t total_lmr_research_depth = 0;

    // Always here
    std::uint64_t nodes_searched = 0;
};

extern BotDebugData bot_debug;
#endif

// Orders moves at once, instead of using PickBestMove
#define ORDER_MOVES

#include "chess.hpp"
#include "core/eval.hpp"
#include "core/scoring.hpp"
#include "core/transposition_table.hpp"
#include "core/killer_table.hpp"
#include "core/threads/multithread.hpp"


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
    };

    ChessBoardEvaluator evaluator;

    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<MoveOrder> move_orderer;

    std::atomic<int64_t> time_limit_ms{0};
    std::chrono::steady_clock::time_point search_start;
    bool time_up = false;
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> worker_time_up{false};
    std::atomic<bool>* external_stop_requested = nullptr;

    std::shared_ptr<TranspositionTable> transposition_table;
    std::shared_ptr<KillerMoves> killer_moves;

    // MultiThreadManager
    MultiThreadManager<MoveResult, SearchParams> manager;

    std::atomic<uint64_t> nodes_searched{0};

    inline uint64_t GetPositionKey() const
    {
        return board->GetZobristHash();
    }

    Evaluation MainSearch(Evaluation alpha, Evaluation beta, int depth, int ply);
    Evaluation SearchCore(const SearchParams& params);
    MoveResult EvaluateRootMove(const SearchParams& params);

    int DepthExtension(const Move& move);

    ChessBot(std::shared_ptr<ChessBoard> board, bool start_manager);

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

#ifdef DEBUG
void PrintBotDebug();
void ClearBotDebug();
#endif
