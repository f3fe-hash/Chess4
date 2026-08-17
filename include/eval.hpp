#pragma once

#include <cstdint>

#include <memory>
#include <unordered_map>
#include <algorithm>

#include "transposition_table.hpp"
#include "chess.hpp"

using Evaluation = float;

extern const Evaluation PAWN_VALUE;
extern const Evaluation KNIGHT_VALUE;
extern const Evaluation BISHOP_VALUE;
extern const Evaluation ROOK_VALUE;
extern const Evaluation QUEEN_VALUE;
extern const Evaluation CHECKMATE_SCORE;

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

    bool CompareMoves(
        const Move& move1,
        const Move& move2,
        int depth);

    Evaluation QuiesenceSearchMain(int depth, Evaluation alpha, Evaluation beta);

public:
    ChessBoardEvaluation() {} // default constructor
    ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board, std::shared_ptr<TranspositionTable> tt_);
    ~ChessBoardEvaluation();

    // Move ordering / move sorting
    Evaluation MoveOrderScore(const Move& move, int depth);
    void SortMoves(std::vector<Move>& moves, int depth);

    // Evaluation functions
    Evaluation EvaluatePieceValues();
    Evaluation EvaluatePSTs();
    Evaluation ComputeMopupBonus();
    Evaluation EvaluateMobility();

    // Position evaluation.
    Evaluation EvaluatePosition();
    Evaluation QuiesenceSearch(int depth);
};
