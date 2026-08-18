#pragma once

#include <cstdint>
#include <vector>

#include <memory>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <unordered_map>

#include "chess.hpp"
#include "core/eval.hpp"
#include "core/transposition_table.hpp"
#include "core/scoring.hpp"


struct MoveResult
{
    Move move;
    Evaluation eval;
    uint64_t nodes_searched;
    int depth;
};


using DurationMs = std::chrono::milliseconds;


class ChessBot
{
    ChessBoardEvaluator evaluator;

    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<MoveOrder> move_orderer;

    DurationMs time_limit;
    std::chrono::steady_clock::time_point search_start;
    bool time_up;

    std::shared_ptr<TranspositionTable> transposition_table;

    uint64_t nodes_searched;

    inline uint64_t GetPositionKey() const
    {
        return board->GetZobristHash();
    }

    Evaluation MainSearch(Evaluation alpha, Evaluation beta, int depth, int ply);
    Evaluation SearchCore(Evaluation& alpha, Evaluation& beta, int depth, int ply, Move move, int move_idx);

    int DepthExtension(const Move& move);

public:
    ChessBot(std::shared_ptr<ChessBoard> board);
    ~ChessBot();

    void SetTimeLimit(DurationMs _time_limit);
    DurationMs GetTimeLimit()
    { return time_limit; }

    inline Evaluation Evaluate()
    { return evaluator.QuiescenceSearch(); }

    inline Evaluation EvaluateRaw()
    { return evaluator.EvaluatePosition(); }

    inline size_t GetTranspositionTableSize()
    { return transposition_table->GetNumEntries(); }

    MoveResult Search(int min_depth, int max_depth);
};
