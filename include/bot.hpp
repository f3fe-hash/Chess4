#pragma once

#include <cstdint>
#include <vector>

#include <memory>
#include <chrono>

#include "chess.hpp"
#include "eval.hpp"

#define min(a, b) ( (a) < (b) ? (a) : (b) )
#define max(a, b) ( (a) > (b) ? (a) : (b) )

struct MoveResult
{
    Move move;
    Evaluation eval;
    uint64_t nodes_searched;
};

using DurationMs = std::chrono::duration<std::chrono::microseconds>;

class ChessBot
{
    ChessBoardEvaluation evaluator;

    std::shared_ptr<ChessBoard> board;

    DurationMs time_limit;

    uint64_t nodes_searched;

    Evaluation MainSearch(Evaluation alpha, Evaluation beta, int depth);

public:
    ChessBot(std::shared_ptr<ChessBoard> _board);
    ~ChessBot();

    void SetTimeLimit(DurationMs _time_limit);

    Evaluation Evaluate();

    MoveResult Search(int max_depth);
};
