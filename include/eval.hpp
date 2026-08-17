#pragma once

#include <cstdint>

#include <memory>
#include <unordered_map>
#include <algorithm>

#include "transposition_table.hpp"
#include "chess.hpp"

using Evaluation = float;

extern const int PAWN_VALUE;
extern const int KNIGHT_VALUE;
extern const int BISHOP_VALUE;
extern const int ROOK_VALUE;
extern const int QUEEN_VALUE;
extern const int CHECKMATE_SCORE;

class ChessBoardEvaluation
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<TranspositionTable> transposition_table;

    Square __fix_pst_square(Square square);

    inline int GetEndgamePhase()
    {
        int material = 0;

        material += board->CountQueens()  * QUEEN_VALUE;
        material += board->CountRooks()   * ROOK_VALUE;
        material += board->CountBishops() * BISHOP_VALUE;
        material += board->CountKnights() * KNIGHT_VALUE;

        // 0 = middlegame
        // 256 = pure king/pawn endgame
        constexpr int ENDGAME_MATERIAL = 2000;

        int phase = 256 - (material * 256 / ENDGAME_MATERIAL);

        return std::clamp(phase, 0, 256);
    }

    inline int distance_to_edge(Square sq)
    {
        int x = get_piece_x(sq);
        int y = get_piece_y(sq);

        return std::min({
            x,
            7 - x,
            y,
            7 - y
        });
    }

    Evaluation QuiesenceSearchMain(int depth, Evaluation alpha, Evaluation beta);

public:
    ChessBoardEvaluation() {} // default constructor
    ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board, std::shared_ptr<TranspositionTable> tt_);
    ~ChessBoardEvaluation();

    // Move ordering.
    Evaluation MoveOrderScore(const Move& move);

    // Evaluation functions
    Evaluation EvaluatePieceValues();
    Evaluation EvaluatePSTs();
    Evaluation ComputeMopupBonus();
    Evaluation EvaluateMobility();

    // Position evaluation.
    Evaluation EvaluatePosition();
    Evaluation QuiesenceSearch(int depth);
};
