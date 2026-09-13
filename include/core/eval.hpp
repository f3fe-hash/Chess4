#pragma once

#include <cstdint>

#include <memory>
#include <unordered_map>
#include <algorithm>

#include "chess.hpp"

#include "core/transposition_table.hpp"
#include "core/move_ordering.hpp"
#include "core/scoring.hpp"

class ChessBoardEvaluator
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<TranspositionTable> transposition_table;
    std::shared_ptr<MoveOrder> move_orderer;

    Square __fix_pst_square(Square square);

    inline int distance_to_edge(Square sq)
    {
        // Returns the heuristic distance between a square and the nearest edge.
        int x = get_piece_x(sq);
        int y = get_piece_y(sq);

        return std::min({
            x,
            7 - x,
            y,
            7 - y
        });
    }

    Evaluation QuiescenceSearchMain(
        Evaluation alpha,
        Evaluation beta,
        int depth);

public:
    ChessBoardEvaluator() {} // default constructor
    ChessBoardEvaluator(
        std::shared_ptr<ChessBoard> board,
        std::shared_ptr<TranspositionTable> tt,
        std::shared_ptr<MoveOrder> move_orderer
    );
    ~ChessBoardEvaluator();

    // Evaluation functions
    Evaluation EvaluatePieceValues();
    Evaluation EvaluatePSTs();
    Evaluation ComputeMopupBonus();
    Evaluation EvaluateMobility();

    // Position evaluation.
    Evaluation EvaluatePosition();
    Evaluation QuiescenceSearch();

    // Game phase
    inline int GetEndgamePhase() const
    {
        // Returns the approximate game phase:
        //
        //   0   = opening
        //   128 = middlegame
        //   256 = endgame
        //
        // The phase is based on non-pawn material remaining.

        int material = 0;

        material += board->CountQueens()  * GetQueenValue();
        material += board->CountRooks()   * GetRookValue();
        material += board->CountBishops() * GetBishopValue();
        material += board->CountKnights() * GetKnightValue();

        // Approximate non-pawn material at the beginning of a game.
        //
        // 2 queens  = 2 * Q
        // 4 rooks   = 4 * R
        // 4 bishops = 4 * B
        // 4 knights = 4 * N
        //
        // This should ideally be calculated from your actual starting
        // piece values rather than hard-coded.
        const int OPENING_MATERIAL =
            2 * QUEEN_VALUE_OP +
            4 * ROOK_VALUE_OP +
            4 * BISHOP_VALUE_OP +
            4 * KNIGHT_VALUE_OP;

        constexpr int ENDGAME_MATERIAL = 2000;

        // More material -> lower phase.
        //
        // Opening material:
        //     phase = 0
        //
        // Endgame threshold:
        //     phase = 256
        //
        // Material below ENDGAME_MATERIAL is considered fully
        // endgame and therefore remains at 256.
        if (material >= OPENING_MATERIAL)
            return 0;

        if (material <= ENDGAME_MATERIAL)
            return 256;

        const int phase =
            (OPENING_MATERIAL - material) * 256 /
            (OPENING_MATERIAL - ENDGAME_MATERIAL);

        return std::clamp(phase, 0, 256);
    }

    inline bool IsEndgame() const
    { return GetEndgamePhase() >= 171; }

    inline bool IsMiddlegame() const
    {
        const int phase = GetEndgamePhase();
        return phase < 171 && phase > 85;
    }

    inline bool IsOpening() const
    { return GetEndgamePhase() <= 85; }
};
