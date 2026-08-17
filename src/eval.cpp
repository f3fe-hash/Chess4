#include "eval.hpp"
#include <algorithm>


const int CHECKMATE_SCORE = 1000000000;


// Piece values
const int PAWN_VALUE    = 100;
const int KNIGHT_VALUE  = 300;
const int BISHOP_VALUE  = 320;
const int ROOK_VALUE    = 500;
const int QUEEN_VALUE   = 900;

// Evaluation multipliers
const float PIECE_VALUE_MULTIPLIER = 0.5;
const float PST_EVAL_MULTIPLIER    = 1.2;
const float MOP_UP_MULTIPLIER      = 1.3;
const float MOBILITY_MULTIPLIER    = 1.1;


//
//  Piece Square Tables (PSTs)
//


const int PAWN_PST[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    90, 90, 90, 90, 90, 90, 90, 90,
    80, 80, 80, 80, 80, 80, 80, 80,
    50, 50, 50, 50, 50, 50, 50, 50,
    30, 30, 30, 45, 45, 30, 30, 30,
    20, 20, 20, 25, 25, 20, 20, 20,
     5,  5,  5,  5,  5,  5,  5,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int KNIGHT_PST[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20,   0,   5,   5,   0, -20, -40,
    -30,   5,  10,  15,  15,  10,   5, -30,
    -30,   0,  15,  20,  20,  15,   0, -30,
    -30,   5,  15,  20,  20,  15,   5, -30,
    -30,   0,  10,  15,  15,  10,   0, -30,
    -40, -20,   0,   0,   0,   0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50
};

const int BISHOP_PST[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,  10,  10,   5,   0, -10,
    -10,   5,   5,  10,  10,   5,   5, -10,
    -10,   0,  10,  10,  10,  10,   0, -10,
    -10,  10,  10,  10,  10,  10,  10, -10,
    -10,   5,   0,   0,   0,   0,   5, -10,
    -20, -10, -10, -10, -10, -10, -10, -20
};

const int ROOK_PST[64] = {
      0,   0,   0,   5,   5,   0,   0,   0,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
     -5,   0,   0,   0,   0,   0,   0,  -5,
      5,  10,  10,  10,  10,  10,  10,   5,
      0,   0,   0,   0,   0,   0,   0,   0
};

const int QUEEN_PST[64] = {
    -20, -10, -10,  -5,  -5, -10, -10, -20,
    -10,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   0, -10,
     -5,   0,   5,   5,   5,   5,   0,  -5,
      0,   0,   5,   5,   5,   5,   0,  -5,
    -10,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,   0,   0,   0,   0, -10,
    -20, -10, -10,  -5,  -5, -10, -10, -20
};

const int KING_PST[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -10, -20, -20, -20, -20, -20, -20, -10,
     20,  20,   0,   0,   0,   0,  20,  20,
     20,  30,  10,   0,   0,  10,  30,  20
};

const int KING_ENDGAME_PST[64] = {
    -50, -40, -30, -20, -20, -30, -40, -50,
    -40, -20, -10,   0,   0, -10, -20, -40,
    -30, -10,  10,  20,  20,  10, -10, -30,
    -20,   0,  20,  30,  30,  20,   0, -20,
    -20,   0,  20,  30,  30,  20,   0, -20,
    -30, -10,  10,  20,  20,  10, -10, -30,
    -40, -20, -10,   0,   0, -10, -20, -40,
    -50, -40, -30, -20, -20, -30, -40, -50
};


ChessBoardEvaluation::ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board, std::shared_ptr<TranspositionTable> tt_) :
    board(_board), transposition_table(tt_)
{}


ChessBoardEvaluation::~ChessBoardEvaluation()
{}


Square ChessBoardEvaluation::__fix_pst_square(Square square)
{
    if (board->GetTurnColor() == TURN_WHITE)
        return square;
    
    uint8_t x = get_piece_x(square);
    uint8_t y = get_piece_y(square);

    return flatten_xy(x, 7 - y);
}


Evaluation ChessBoardEvaluation::EvaluatePieceValues()
{
    // Evaluate piece values for the current turn color.

    Evaluation eval = 0;

    eval += PAWN_VALUE * board->CountPawns();
    eval += KNIGHT_VALUE * board->CountKnights();
    eval += BISHOP_VALUE * board->CountBishops();
    eval += ROOK_VALUE * board->CountRooks();
    eval += QUEEN_VALUE * board->CountQueens();

    return eval;
}


Evaluation ChessBoardEvaluation::EvaluatePSTs()
{
    Evaluation eval = 0;

    for (Square square = 0; square < 64; ++square)
    {
        Piece piece = board->GetPieceAt(square);
        if (piece == NULL_PIECE)
            continue;

        bool pieceIsFriendly = (board->GetTurnColor() == TURN_WHITE)
            ? bool(piece & PIECE_COLOR_WHITE)
            : bool(piece & PIECE_COLOR_BLACK);

        if (!pieceIsFriendly)
            continue;

        Square pst_square = __fix_pst_square(square);
        int phase = GetEndgamePhase();

        switch (piece & 0x07)
        {
            case PIECE_TYPE_PAWN:
                eval += PAWN_PST[pst_square];
                break;

            case PIECE_TYPE_KNIGHT:
                eval += KNIGHT_PST[pst_square];
                break;

            case PIECE_TYPE_BISHOP:
                eval += BISHOP_PST[pst_square];
                break;

            case PIECE_TYPE_ROOK:
                eval += ROOK_PST[pst_square];
                break;

            case PIECE_TYPE_QUEEN:
                eval += QUEEN_PST[pst_square];
                break;

            case PIECE_TYPE_KING:
            {
                int king_score =
                    ((256 - phase) * KING_PST[pst_square] +
                    phase * KING_ENDGAME_PST[pst_square]) / 256;
                
                eval += king_score;
                break;
            }

            default:
                break;
        }
    }

    return eval;
}


Evaluation ChessBoardEvaluation::ComputeMopupBonus()
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

        switch (type)
        {
            case PIECE_TYPE_PAWN:
                value = PAWN_VALUE;
                break;

            case PIECE_TYPE_KNIGHT:
                value = KNIGHT_VALUE;
                break;

            case PIECE_TYPE_BISHOP:
                value = BISHOP_VALUE;
                break;

            case PIECE_TYPE_ROOK:
                value = ROOK_VALUE;
                break;

            case PIECE_TYPE_QUEEN:
                value = QUEEN_VALUE;
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

        //int major_distance = 8;

        //for (Square major : white_majors)
        //{
        //    major_distance =
        //        std::min(
        //            major_distance,
        //            chebyshev_distance(major, black_king)
        //        );
        //}

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
        //white_bonus +=
        //    major_distance * 10;

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

        //int major_distance = 8;

        //for (Square major : black_majors)
        //{
        //    major_distance =
        //        std::min(
        //           major_distance,
        //            chebyshev_distance(major, white_king)
        //        );
        //}

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
        //black_bonus +=
        //    major_distance * 10;

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


Evaluation ChessBoardEvaluation::EvaluateMobility()
{
    
}


static inline Evaluation PieceValue(Piece piece)
{
    switch (piece & 0x07)
    {
        case PIECE_TYPE_PAWN:   return PAWN_VALUE;
        case PIECE_TYPE_KNIGHT: return KNIGHT_VALUE;
        case PIECE_TYPE_BISHOP: return BISHOP_VALUE;
        case PIECE_TYPE_ROOK:   return ROOK_VALUE;
        case PIECE_TYPE_QUEEN:  return QUEEN_VALUE;
        case PIECE_TYPE_KING:   return 1000;
        default:                return 0;
    }
}


Evaluation ChessBoardEvaluation::MoveOrderScore(const Move& move)
{
    if (move.captured == NULL_PIECE)
        return 0;
    
    // Lookup in transposition table.
    ZobristHash key = board->GetZobristHash();
    if (transposition_table->keyIsStored(key))
    {
        // It is stored! If this is the stored move, give it a higher ranking.
        TranspositionTableEntry entry = transposition_table->getKey(key);
        if (entry.best_move == move)
            return 2000;
    }

    Evaluation victim = PieceValue(move.captured);
    Evaluation attacker = PieceValue(move.moved);

    return (victim * 10) - attacker;
}


Evaluation ChessBoardEvaluation::EvaluatePosition()
{
    bool turn = board->GetTurnColor();

    Evaluation eval_white = 0;
    Evaluation eval_black = 0;

    // ------------------------------------------------------------
    // White evaluation.
    // ------------------------------------------------------------

    board->SetTurnColor(TURN_WHITE);

    eval_white += PIECE_VALUE_MULTIPLIER * EvaluatePieceValues();
    eval_white += PST_EVAL_MULTIPLIER * EvaluatePSTs();

    //if (board->IsCheckMate())
    //    eval_white -= 100000;
    if (board->IsCheck())
        eval_white -= 100;

    // ------------------------------------------------------------
    // Black evaluation.
    // ------------------------------------------------------------

    board->SetTurnColor(TURN_BLACK);

    eval_black += PIECE_VALUE_MULTIPLIER * EvaluatePieceValues();
    eval_black += PST_EVAL_MULTIPLIER * EvaluatePSTs();

    //if (board->IsCheckMate())
    //    eval_black -= 100000;
    if (board->IsCheck())
        eval_black -= 100;

    // Restore original turn.
    board->SetTurnColor(turn);

    Evaluation base = eval_white - eval_black;

    base +=
        MOP_UP_MULTIPLIER * ComputeMopupBonus();

    return base;
}


Evaluation ChessBoardEvaluation::QuiesenceSearchMain(int depth, Evaluation alpha, Evaluation beta)
{
    // Evaluation a position only when all captures have been made.

    // Get legal captures.
    std::vector<Move> captures = board->GetLegalCaptures();

    // No more captures.
    if (captures.size() == 0)
        return EvaluatePosition();

    // Checkmate.
    if (board->IsCheckMate())
        return board->GetTurnColor() ? -CHECKMATE_SCORE : CHECKMATE_SCORE;
    
    // Stalemate.
    if (board->IsStaleMate())
        return 0;
    
    // Max depth.
    if (depth == 0)
        return EvaluatePosition();
    

    bool maximizing = board->GetTurnColor() == TURN_WHITE;
    Evaluation best_eval = maximizing ? INT_MIN : INT_MAX;

    for (Move capture : captures)
    {
        board->MakeMove(capture);

        Evaluation eval = QuiesenceSearch(depth - 1);

        board->UndoMove(capture);


        if (maximizing)
        {
            if (eval > best_eval)
            {
                best_eval = eval;
            }

            alpha = std::max(alpha, eval);
        }
        else
        {
            if (eval < best_eval)
            {
                best_eval = eval;
            }

            beta = std::min(beta, eval);
        }

        // Prune.
        if (alpha >= beta)
            break;
    }

    return best_eval;
}


Evaluation ChessBoardEvaluation::QuiesenceSearch(int depth)
{
    Evaluation alpha = INT_MIN;
    Evaluation beta = INT_MAX;

    return QuiesenceSearchMain(depth, alpha, beta);
}

