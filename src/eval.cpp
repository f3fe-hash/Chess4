#include "eval.hpp"


// Piece values
const int PAWN_VALUE    = 100;
const int KNIGHT_VALUE  = 300;
const int BISHOP_VALUE  = 320;
const int ROOK_VALUE    = 500;
const int QUEEN_VALUE   = 900;

// Evaluation multipliers
const int PIECE_VALUE_MULTIPLIER = 1;


ChessBoardEvaluation::ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board) : board(_board)
{}


ChessBoardEvaluation::~ChessBoardEvaluation()
{}


Evaluation ChessBoardEvaluation::EvaluatePieceValues()
{
    Evaluation eval = 0;

    eval += PAWN_VALUE * board->CountPawns();
    eval += KNIGHT_VALUE * board->CountKnights();
    eval += BISHOP_VALUE * board->CountBishops();
    eval += ROOK_VALUE * board->CountRooks();
    eval += QUEEN_VALUE * board->CountQueens();

    // evaluation is flipped in `EvaluatePosition` function.
    return eval;
}


Evaluation ChessBoardEvaluation::EvaluatePosition()
{
    Evaluation eval = 0;

    eval += PIECE_VALUE_MULTIPLIER * EvaluatePieceValues();

    return (board->GetTurnColor() == TURN_WHITE) ? eval : -eval;
}

