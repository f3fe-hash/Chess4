#pragma once

#include <vector>
#include <algorithm>
#include <memory>

#include "chess.hpp"
#include "core/transposition_table.hpp"
#include "core/scoring.hpp"

class MoveOrder
{
    std::shared_ptr<TranspositionTable> transposition_table;
    std::shared_ptr<ChessBoard> board;

    Evaluation PieceValue(Piece piece);

    // Generate a score for move ordering.
    Evaluation MoveOrderScore(
        const Move& move,
        const Move& tt_move,
        const int depth);

public:
    MoveOrder() {}

    MoveOrder(
        std::shared_ptr<TranspositionTable> transposition_table,
        std::shared_ptr<ChessBoard> board) :
        transposition_table(transposition_table), board(board)
    {}
    
    ~MoveOrder() {}

    void OrderMoves(std::vector<Move>& moves, int depth);
};
