#pragma once

#include <cstdint>
#include <memory>

#include "chess.hpp"

using Evaluation = float;

class ChessBoardEvaluation
{
    std::shared_ptr<ChessBoard> board;

    Square __fix_pst_square(Square square);

    Evaluation EvaluatePieceValues();

    Evaluation EvaluatePSTs();

public:
    ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board);
    ~ChessBoardEvaluation();

    Evaluation EvaluatePosition();
};
