#pragma once

#include <cstdint>
#include <vector>

#include <memory>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <unordered_map>

#include "chess.hpp"
#include "eval.hpp"
#include "transposition_table.hpp"


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
    ChessBoardEvaluation evaluator;

    std::shared_ptr<ChessBoard> board;

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

    int DepthExtension();

public:
    ChessBot(std::shared_ptr<ChessBoard> _board);
    ~ChessBot();

    void SetTimeLimit(DurationMs _time_limit);
    DurationMs GetTimeLimit()
    { return time_limit; }

    inline Evaluation Evaluate()
    { return evaluator.QuiesenceSearch(10); }

    inline Evaluation EvaluateRaw()
    { return evaluator.EvaluatePosition(); }

    inline size_t GetTranspositionTableSize()
    { return transposition_table->GetNumEntries(); }

    MoveResult Search(int min_depth, int max_depth);
};
