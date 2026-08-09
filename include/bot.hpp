#pragma once

#include <cstdint>
#include <vector>

#include <memory>
#include <chrono>
#include <algorithm>

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

class ChessBot
{
    ChessBoardEvaluation evaluator;

    std::shared_ptr<ChessBoard> board;

    DurationMs time_limit;
    std::chrono::steady_clock::time_point search_start;
    bool time_up;

    uint64_t nodes_searched;

    Evaluation MainSearch(Evaluation alpha, Evaluation beta, int depth);

    bool CompareMoves(const Move& move1, const Move& move2);
    void SortMoves(std::vector<Move>& moves);

    int DepthExtension();

public:
    ChessBot(std::shared_ptr<ChessBoard> _board);
    ~ChessBot();

    void SetTimeLimit(DurationMs _time_limit);

    Evaluation Evaluate();

    MoveResult Search(int max_depth);
};
