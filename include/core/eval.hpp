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

    // Endgame
    inline int GetEndgamePhase()
    {
        int material = 0;

        material += board->CountQueens()  * GetQueenValue();
        material += board->CountRooks()   * GetRookValue();
        material += board->CountBishops() * GetBishopValue();
        material += board->CountKnights() * GetKnightValue();

        // 0 = middlegame
        // 256 = pure king/pawn endgame
        constexpr int ENDGAME_MATERIAL = 2000;

        int phase = 256 - (material * 256 / ENDGAME_MATERIAL);

        return std::clamp(phase, 0, 256);
    }
    
    inline bool IsEndgame()
    { return GetEndgamePhase() > 180; }
};
