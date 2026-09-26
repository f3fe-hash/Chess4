#include "core/eval/eval.hpp"

//
//  Evaluation multipliers
// 

const float PIECE_VALUE_MULTIPLIER      = 1; // Keeping pieces safe
const float PST_EVAL_MULTIPLIER         = 1.5; // Piece positioning
const float MOP_UP_MULTIPLIER           = 1.2; // Endgames: push king to edges

// Note: captures evaluation are ON TOP of generic moves.
const float MOBILITY_MULTIPLIER         = 0.9;
const float MOBILITY_MOVE_MULTIPLIER    = 1; // Generic moves don't count for much.
const float MOBILITY_CAPTURE_MULTIPLIER = 2; // Captures are better than generic moves.



//
//  Precomputed Data
//


uint8_t EdgeDistances[64];
uint8_t BlackPSTIndexes[64];


//
//  Piece Square Tables (PSTs)
//


// ============================================================
// PAWN
// ============================================================

// Early game:
// Establish the center, create useful pawn structure, and avoid
// unnecessary pawn moves on the wings.
const int8_t PAWN_EARLYGAME_PST[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
    10,  10,  10,  15,  15,  10,  10,  10,
     5,   5,   8,  15,  15,   8,   5,   5,
     5,   5,  8,  20,  20,   8,   5,   5,
     5,   5,   8,  20,  20,   8,   5,   5,
     5,   5,   5,  10,  10,   5,   5,   5,
     5,   5,   5,   5,   5,   5,   5,   5,
     0,   0,   0,   0,   0,   0,   0,   0
};

// Endgame:
// Advancement becomes much more valuable. Central pawns and pawns
// deep in enemy territory receive increasingly large bonuses.
const int8_t PAWN_ENDGAME_PST[64] = {
     0,   0,   0,   0,   0,   0,   0,   0,
    15,  15,  15,  20,  20,  15,  15,  15,
    20,  20,  25,  30,  30,  25,  20,  20,
    30,  30,  35,  40,  40,  35,  30,  30,
    45,  45,  50,  55,  55,  50,  45,  45,
    60,  60,  65,  70,  70,  65,  60,  60,
    80,  80,  85,  90,  90,  85,  80,  80,
     0,   0,   0,   0,   0,   0,   0,   0
};


// ============================================================
// KNIGHT
// ============================================================

// Early game:
// Strongly discourage the rim and reward central development.
const int8_t KNIGHT_EARLYGAME_PST[64] = {
    -50, -35, -25, -20, -20, -25, -35, -50,
    -35, -15,   0,   5,   5,   0, -15, -35,
    -25,   0,  10,  15,  15,  10,   0, -25,
    -20,   5,  15,  20,  20,  15,   5, -20,
    -20,   5,  15,  20,  20,  15,   5, -20,
    -25,   0,  10,  15,  15,  10,   0, -25,
    -35, -15,   0,   5,   5,   0, -15, -35,
    -50, -35, -25, -20, -20, -25, -35, -50
};

// Endgame:
// Centralization still matters, but there is less emphasis on
// initial development and more on broad attacking coverage.
const int8_t KNIGHT_ENDGAME_PST[64] = {
    -40, -25, -15, -10, -10, -15, -25, -40,
    -25, -10,   0,   5,   5,   0, -10, -25,
    -15,   0,  10,  15,  15,  10,   0, -15,
    -10,   5,  15,  20,  20,  15,   5, -10,
    -10,   5,  15,  20,  20,  15,   5, -10,
    -15,   0,  10,  15,  15,  10,   0, -15,
    -25, -10,   0,   5,   5,   0, -10, -25,
    -40, -25, -15, -10, -10, -15, -25, -40
};


// ============================================================
// BISHOP
// ============================================================

// Early game:
// Reward useful development and active diagonals while avoiding
// repeated bishop moves.
const int8_t BISHOP_EARLYGAME_PST[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   5,   5,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   5,   5,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

// Endgame:
// Open positions make bishop activity and centralization more important.
const int8_t BISHOP_ENDGAME_PST[64] = {
    -15, -10,  -5,  -5,  -5,  -5, -10, -15,
    -10,   0,   5,   8,   8,   5,   0, -10,
     -5,   5,  10,  15,  15,  10,   5,  -5,
     -5,   8,  15,  20,  20,  15,   8,  -5,
     -5,   8,  15,  20,  20,  15,   8,  -5,
     -5,   5,  10,  15,  15,  10,   5,  -5,
    -10,   0,   5,   8,   8,   5,   0, -10,
    -15, -10,  -5,  -5,  -5,  -5, -10, -15
};


// ============================================================
// ROOK
// ============================================================

// Early game:
// Keep rooks connected and reward getting them onto useful files,
// while avoiding premature rook adventures.
const int8_t ROOK_EARLYGAME_PST[64] = {
     0,   0,   5,   5,   5,   5,   0,   0,
     0,   0,   0,   5,   5,   0,   0,   0,
     0,   0,   0,   5,   5,   0,   0,   0,
     0,   0,   0,   5,   5,   0,   0,   0,
     0,   0,   0,   5,   5,   0,   0,   0,
     5,   5,   5,  10,  10,   5,   5,   5,
    10,  10,  10,  15,  15,  10,  10,  10,
     5,   5,  10,  15,  15,  10,   5,   5
};

// Endgame:
// Rooks become highly valuable on open files, the 7th rank, and
// behind advanced pawns.
const int8_t ROOK_ENDGAME_PST[64] = {
     0,   0,   5,   5,   5,   5,   0,   0,
     5,   5,  10,  10,  10,  10,   5,   5,
    10,  10,  15,  15,  15,  15,  10,  10,
    15,  15,  20,  20,  20,  20,  15,  15,
    20,  20,  25,  25,  25,  25,  20,  20,
    30,  30,  35,  35,  35,  35,  30,  30,
    45,  45,  50,  55,  55,  50,  45,  45,
    10,  10,  15,  20,  20,  15,  10,  10
};


// ============================================================
// QUEEN
// ============================================================

// Early game:
// Discourage early queen adventures while allowing useful central
// squares after development.
const int8_t QUEEN_EARLYGAME_PST[64] = {
    -25, -15, -10,  -5,  -5, -10, -15, -25,
    -15,  -5,   0,   0,   0,   0,  -5, -15,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,  10,  10,   5,   0,  -5,
     -5,   0,   5,  10,  10,   5,   0,  -5,
    -10,   0,   5,   5,   5,   5,   0, -10,
    -15,  -5,   0,   0,   0,   0,  -5, -15,
    -25, -15, -10,  -5,  -5, -10, -15, -25
};

// Endgame:
// With fewer pieces on the board, queen activity and centralization
// become substantially more important.
const int8_t QUEEN_ENDGAME_PST[64] = {
    -10,  -5,   0,   5,   5,   0,  -5, -10,
     -5,   0,   5,  10,  10,   5,   0,  -5,
      0,   5,  10,  15,  15,  10,   5,   0,
      5,  10,  15,  20,  20,  15,  10,   5,
      5,  10,  15,  20,  20,  15,  10,   5,
      0,   5,  10,  15,  15,  10,   5,   0,
     -5,   0,   5,  10,  10,   5,   0,  -5,
    -10,  -5,   0,   5,   5,   0,  -5, -10
};


// ============================================================
// KING
// ============================================================

// Early game:
// Get safe and castle. Central squares are heavily discouraged.
const int8_t KING_EARLYGAME_PST[64] = {
    -30, -40, -50, -60, -60, -50, -40, -30,
    -30, -40, -50, -60, -60, -50, -40, -30,
    -30, -40, -50, -60, -60, -50, -40, -30,
    -30, -40, -50, -60, -60, -50, -40, -30,
    -20, -30, -40, -50, -50, -40, -30, -20,
    -10, -20, -20, -30, -30, -20, -20, -10,
     20,  25,   5,   0,   0,   5,  25,  20,
     25,  15,  30,   5,   5,  30,  15,  25
};

// Endgame:
// The king becomes an active fighting piece. Centralization is
// strongly rewarded.
const int8_t KING_ENDGAME_PST[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -40, -20, -10,   0,   0, -10, -20, -40,
    -30, -10,  10,  20,  20,  10, -10, -30,
    -20,   0,  20,  30,  30,  20,   0, -20,
    -20,   0,  20,  30,  30,  20,   0, -20,
    -30, -10,  10,  20,  20,  10, -10, -30,
    -40, -20, -10,   0,   0, -10, -20, -40,
    -50, -40, -30, -20, -20, -30, -40, -50
};


ChessBoardEvaluator::ChessBoardEvaluator(
    std::shared_ptr<ChessBoard> board,
    std::shared_ptr<TranspositionTable> tt,
    std::shared_ptr<MoveOrder> move_orderer
) :
    board(board), transposition_table(tt), move_orderer(move_orderer)
#ifdef USE_EXPR_AI
    , model(board)
#endif
{
}


ChessBoardEvaluator::~ChessBoardEvaluator()
{}


Square ChessBoardEvaluator::GetPSTIndex(Square square)
{
    if (board->GetTurnColor() == TURN_WHITE)
        return square;
    
    return BlackPSTIndexes[square];
}


inline int8_t distance_to_edge(Square sq)
{
    return EdgeDistances[sq];
}


void ChessBoardEvaluator::ComputeDistancesToEdge()
{
    for (Square sq = 0; sq < 64; sq++)
    {
        int8_t x = get_piece_x(sq);
        int8_t y = get_piece_y(sq);

        uint8_t distance = (uint8_t)std::min(
            std::min(x, (int8_t)(7 - x)),
            std::min(y, (int8_t)(7 - y))
        );

        EdgeDistances[sq] = distance;
    }
}


void ChessBoardEvaluator::ComputePSTIndexes()
{
    for (Square sq = 0; sq < 64; sq++)
    {
        uint8_t x = get_piece_x(sq);
        uint8_t y = get_piece_y(sq);

        uint8_t BlackPSTIndex = flatten_xy(x, 7 - y);
        BlackPSTIndexes[sq] = BlackPSTIndex;
    }
}


Evaluation ChessBoardEvaluator::EvaluatePieceValues()
{
    // Evaluate piece values for the current turn color.

    Evaluation eval = 0;

    SetEndgamePhase(GetEndgamePhase());
    eval += GetPawnValue() * board->CountPawns();
    eval += GetKnightValue() * board->CountKnights();
    eval += GetBishopValue() * board->CountBishops();
    eval += GetRookValue() * board->CountRooks();
    eval += GetQueenValue() * board->CountQueens();

    return eval;
}


Evaluation ChessBoardEvaluator::EvaluatePSTs()
{
    Evaluation eval = 0;
    
    const int phase = GetEndgamePhase();

    auto evaluate_pieces =
        [&](Bitboard pieces, const int8_t pst[64], const int8_t endgame_pst[64])
        {
            while (pieces)
            {
                const Square square =
                    GetLSB(pieces);

                pieces &= pieces - 1;

                const Square pst_square = GetPSTIndex(square);

                eval += Interpolate(pst[pst_square], endgame_pst[pst_square], 256 - phase) / 256;
            }
        };

    evaluate_pieces(board->GetPawns(), PAWN_EARLYGAME_PST, PAWN_ENDGAME_PST);
    evaluate_pieces(board->GetKnights(), KNIGHT_EARLYGAME_PST, KNIGHT_ENDGAME_PST);
    evaluate_pieces(board->GetBishops(), BISHOP_EARLYGAME_PST, BISHOP_ENDGAME_PST);
    evaluate_pieces(board->GetRooks(), ROOK_EARLYGAME_PST, ROOK_ENDGAME_PST);
    evaluate_pieces(board->GetQueens(), QUEEN_EARLYGAME_PST, QUEEN_ENDGAME_PST);
    evaluate_pieces(board->GetKings(), KING_EARLYGAME_PST, KING_ENDGAME_PST);

    return eval;
}


Evaluation ChessBoardEvaluator::ComputeMopupBonus()
{
    Square white_king = 64;
    Square black_king = 64;

    std::vector<Square> white_majors;
    std::vector<Square> black_majors;

    int white_material = 0;
    int black_material = 0;

    int white_nonking = 0;
    int black_nonking = 0;

    for (Square sq = 0; sq < 64; ++sq)
    {
        Piece piece = board->GetPieceAt(sq);

        if (piece == NULL_PIECE)
            continue;

        const int type = piece & 0x07;
        const bool white = bool(piece & PIECE_COLOR_WHITE);

        int value = 0;

        SetEndgamePhase(GetEndgamePhase());
        switch (type)
        {
            case PIECE_TYPE_PAWN:
                value = GetPawnValue();
                break;

            case PIECE_TYPE_KNIGHT:
                value = GetKnightValue();
                break;

            case PIECE_TYPE_BISHOP:
                value = GetBishopValue();
                break;

            case PIECE_TYPE_ROOK:
                value = GetRookValue();
                break;

            case PIECE_TYPE_QUEEN:
                value = GetQueenValue();
                break;

            case PIECE_TYPE_KING:
                break;

            default:
                break;
        }

        if (white)
        {
            white_material += value;

            if (type == PIECE_TYPE_KING)
            {
                white_king = sq;
            }
            else
            {
                ++white_nonking;

                if (type == PIECE_TYPE_ROOK ||
                    type == PIECE_TYPE_QUEEN)
                {
                    white_majors.push_back(sq);
                }
            }
        }
        else
        {
            black_material += value;

            if (type == PIECE_TYPE_KING)
            {
                black_king = sq;
            }
            else
            {
                ++black_nonking;

                if (type == PIECE_TYPE_ROOK ||
                    type == PIECE_TYPE_QUEEN)
                {
                    black_majors.push_back(sq);
                }
            }
        }
    }

    if (white_king >= 64 || black_king >= 64)
        return 0;

    auto chebyshev_distance = [](Square a, Square b) -> int
    {
        int ax = get_piece_x(a);
        int ay = get_piece_y(a);

        int bx = get_piece_x(b);
        int by = get_piece_y(b);

        int dx = std::abs(ax - bx);
        int dy = std::abs(ay - by);

        return std::max(dx, dy);
    };

    Evaluation white_bonus = 0;
    Evaluation black_bonus = 0;

    const Evaluation ROOK_VALUE = GetRookValue();

    //
    // White is winning with a major piece.
    //
    if (black_nonking == 0 &&
        white_material >= ROOK_VALUE &&
        !white_majors.empty())
    {
        int enemy_edge_distance = distance_to_edge(black_king);

        int king_distance =
            chebyshev_distance(white_king, black_king);

        //
        // 1. Force the enemy king toward the edge.
        //
        // Center = distance 3
        // Edge   = distance 0
        //
        white_bonus +=
            (3 - enemy_edge_distance) * 100;

        //
        // 2. Bring our king toward the enemy king.
        //
        white_bonus +=
            (7 - king_distance) * 50;

        //
        // 3. Keep the rook/queen away from the enemy king.
        //
        // A major piece close to the enemy king is more likely
        // to get attacked.
        //
        //
        // 4. Extra major material.
        //
        if (white_material > ROOK_VALUE)
        {
            white_bonus +=
                (white_material - ROOK_VALUE) / 5;
        }
    }

    //
    // Black is winning with a major piece.
    //
    if (white_nonking == 0 &&
        black_material >= ROOK_VALUE &&
        !black_majors.empty())
    {
        int enemy_edge_distance = distance_to_edge(white_king);

        int king_distance =
            chebyshev_distance(black_king, white_king);

        //
        // Force the enemy king toward the edge.
        //
        black_bonus +=
            (3 - enemy_edge_distance) * 100;

        //
        // Bring our king toward the enemy king.
        //
        black_bonus +=
            (7 - king_distance) * 50;

        //
        // Keep the rook/queen away from the enemy king.
        //
        //
        // Extra major material.
        //
        if (black_material > ROOK_VALUE)
        {
            black_bonus +=
                (black_material - ROOK_VALUE) / 5;
        }
    }

    return white_bonus - black_bonus;
}


Evaluation ChessBoardEvaluator::EvaluateMobility()
{
    size_t num_moves;
    size_t num_captures;
    board->GetNumLegalMovesAndCaptures(num_moves, num_captures);

    return
        (num_moves * MOBILITY_MOVE_MULTIPLIER) +
        (num_captures * MOBILITY_CAPTURE_MULTIPLIER);
}


Evaluation ChessBoardEvaluator::EvaluatePosition()
{
    Evaluation TOTAL_MULTIPLIERS = PST_EVAL_MULTIPLIER + MOBILITY_MULTIPLIER;

    bool turn = board->GetTurnColor();

    Evaluation eval_white = 0;
    Evaluation eval_black = 0;
    Evaluation material_balance = 0;

    for (Square square = 0; square < 64; ++square)
    {
        const Piece piece = board->GetPieceAt(square);
        const Evaluation value = [&]() -> Evaluation
        {
            switch (piece & 0x07)
            {
                case PIECE_TYPE_PAWN: return PAWN_VALUE_OP;
                case PIECE_TYPE_KNIGHT: return KNIGHT_VALUE_OP;
                case PIECE_TYPE_BISHOP: return BISHOP_VALUE_OP;
                case PIECE_TYPE_ROOK: return ROOK_VALUE_OP;
                case PIECE_TYPE_QUEEN: return QUEEN_VALUE_OP;
                default: return 0;
            }
        }();

        material_balance +=
            (piece & PIECE_COLOR_WHITE) ? value : -value;
    }

    // ------------------------------------------------------------
    // White evaluation.
    // ------------------------------------------------------------

    board->SetTurnColor(TURN_WHITE);

    eval_white += PST_EVAL_MULTIPLIER       * EvaluatePSTs();
    eval_white += MOBILITY_MULTIPLIER       * EvaluateMobility();

    // ------------------------------------------------------------
    // Black evaluation.
    // ------------------------------------------------------------

    board->SetTurnColor(TURN_BLACK);

    eval_black += PST_EVAL_MULTIPLIER       * EvaluatePSTs();
    eval_black += MOBILITY_MULTIPLIER       * EvaluateMobility();

    // Restore original turn.
    board->SetTurnColor(turn);

    Evaluation base = material_balance + eval_white - eval_black;

    // Mop-up is more useful in the endgame, and is really expensive to calculate.
    if (IsEndgame())
    {
        base +=
            MOP_UP_MULTIPLIER * ComputeMopupBonus();
        TOTAL_MULTIPLIERS += MOP_UP_MULTIPLIER;
    }

    // AI evaluation: Experimental
#ifdef USE_EXPR_AI
    const Evaluation correction = model.Evaluate();
    return (base / TOTAL_MULTIPLIERS) + correction;
#else
    return (base / TOTAL_MULTIPLIERS);
#endif
}


thread_local size_t qsearch_nodes = 0;

Evaluation ChessBoardEvaluator::QuiescenceSearchMain(
    Evaluation alpha,
    Evaluation beta,
    int depth
)
{
    ++qsearch_nodes;

    Move tt_move{};
    const ZobristHash key = board->GetZobristHash();

    bool found;
    const TranspositionTableEntry entry = transposition_table->GetEntry(key, found);
    if (found)
    {
        tt_move = entry.best_move;
    }

    if (board->IsCheck())
    {
        auto moves = board->GetLegalMoves();
        const bool maximizing = board->GetTurnColor() == TURN_WHITE;

        if (moves.empty())
            return maximizing ? -CHECKMATE_SCORE : CHECKMATE_SCORE;

        for (int move_idx = 0;
             move_idx < static_cast<int>(moves.size());
             ++move_idx)
        {
            Move move = move_orderer->PickBestMove(
                moves,
                move_idx,
                tt_move,
                0
            );

            // `MakeMove` edits `move` with castling rights, promption flags, etc. for `UndoMove`
            board->MakeMove(move);

            const Evaluation score =
                QuiescenceSearchMain(alpha, beta, depth - 1);

            board->UndoMove(move);

            if (maximizing)
            {
                if (score >= beta)
                    return score;
                alpha = std::max(alpha, score);
            }
            else
            {
                if (score <= alpha)
                    return score;
                beta = std::min(beta, score);
            }
        }

        return maximizing ? alpha : beta;
    }

    const Evaluation stand_pat = EvaluatePosition();
    const bool maximizing = board->GetTurnColor() == TURN_WHITE;

    if (depth <= 0)
        return stand_pat;

    if (maximizing)
    {
        if (stand_pat >= beta)
            return stand_pat;
        alpha = std::max(alpha, stand_pat);
    }
    else
    {
        if (stand_pat <= alpha)
            return stand_pat;
        beta = std::min(beta, stand_pat);
    }

    if (board->HasPseudoLegalCapture())
        return maximizing ? alpha : beta;

    auto captures = board->GetLegalCaptures();

    for (int move_idx = 0;
         move_idx < static_cast<int>(captures.size());
         ++move_idx)
    {
        Move move = move_orderer->PickBestMove(
            captures,
            move_idx,
            tt_move,
            0
        );

        // `MakeMove` edits `move` with castling rights, promption flags, etc. for `UndoMove`
        board->MakeMove(move);

        const Evaluation score =
            QuiescenceSearchMain(alpha, beta, depth - 1);

        board->UndoMove(move);

        if (maximizing)
        {
            if (score >= beta)
                return score;
            alpha = std::max(alpha, score);
        }
        else
        {
            if (score <= alpha)
                return score;
            beta = std::min(beta, score);
        }
    }

    return maximizing ? alpha : beta;
}


Evaluation ChessBoardEvaluator::QuiescenceSearch()
{
    qsearch_nodes = 0;

    Evaluation alpha = INT_MIN;
    Evaluation beta = INT_MAX;

    return QuiescenceSearchMain(alpha, beta, 100);
}


