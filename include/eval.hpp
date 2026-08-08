#pragma once

#include <cstdint>
#include <memory>

#include "chess.hpp"

using Evaluation = int;

class ChessBoardEvaluation
{
    std::shared_ptr<ChessBoard> board;

    Evaluation EvaluatePieceValues();

public:
    ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board);
    ~ChessBoardEvaluation();

    Evaluation EvaluatePosition();
};
