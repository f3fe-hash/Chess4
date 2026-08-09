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


struct MoveResult
{
    Move move;
    Evaluation eval;
    uint64_t nodes_searched;
    int depth;
};


using DurationMs = std::chrono::milliseconds;


struct TranspositionEntry
{
    Evaluation eval;
    uint8_t depth;

    enum Bound : uint8_t
    {
        EXACT,
        LOWER_BOUND,
        UPPER_BOUND
    } bound;
};


class ChessBot
{
    ChessBoardEvaluation evaluator;

    std::shared_ptr<ChessBoard> board;

    DurationMs time_limit;
    std::chrono::steady_clock::time_point search_start;
    bool time_up;

    std::unordered_map<uint64_t, TranspositionEntry> transposition_table;

    uint64_t nodes_searched;

    inline uint64_t GetPositionKey() const
    {
        return board->GetZobristHash();
    }

    Evaluation MainSearch(Evaluation alpha, Evaluation beta, int depth, int ply);
    Evaluation SearchCore(Evaluation& alpha, Evaluation& beta, int depth, int ply, Move move, int move_idx);

    bool CompareMoves(const Move& move1, const Move& move2);
    void SortMoves(std::vector<Move>& moves);

    int DepthExtension();

public:
    ChessBot(std::shared_ptr<ChessBoard> _board);
    ~ChessBot();

    void SetTimeLimit(DurationMs _time_limit);
    DurationMs GetTimeLimit()
    { return time_limit; }

    Evaluation Evaluate();

    MoveResult Search(int min_depth, int max_depth);
};
