#pragma once

#include <vector>
#include <algorithm>
#include <memory>

#include "chess.hpp"
#include "core/killer_table.hpp"
#include "core/transposition_table.hpp"
#include "core/scoring.hpp"

class MoveOrder
{
    std::shared_ptr<TranspositionTable> transposition_table;
    std::shared_ptr<KillerMoves> killer_moves;
    std::shared_ptr<ChessBoard> board;

    Evaluation PieceValue(Piece piece) const;

    // Generate a score for move ordering.
    Evaluation MoveOrderScore(
        const Move& move,
        const Move& tt_move,
        const int depth,
        const int ply) const;

public:
    MoveOrder() {}

    MoveOrder(
        std::shared_ptr<TranspositionTable> transposition_table,
        std::shared_ptr<KillerMoves> killer_moves,
        std::shared_ptr<ChessBoard> board) :
        transposition_table(transposition_table),
        killer_moves(killer_moves),
        board(board)
    {}
    
    ~MoveOrder() = default;

    void OrderMoves(std::vector<Move>& moves, const int depth, const int ply) const;

    Move PickBestMove(
        std::vector<Move>& moves,
        const int start,
        const Move& tt_move,
        const int depth = 0,
        const int ply = 0) const;
};
