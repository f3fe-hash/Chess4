#include "core/eval.hpp"
#include <algorithm>


// Evaluation multipliers
const float PIECE_VALUE_MULTIPLIER      = 1;
const float PST_EVAL_MULTIPLIER         = 1.2;
const float MOP_UP_MULTIPLIER           = 1.3;
const float MOBILITY_MULTIPLIER         = 0.9;
const float MOBILITY_MOVE_MULTIPLIER    = 2;
const float MOBILITY_CAPTURE_MULTIPLIER = 4;


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


ChessBoardEvaluator::ChessBoardEvaluator(
    std::shared_ptr<ChessBoard> board,
    std::shared_ptr<TranspositionTable> tt,
    std::shared_ptr<MoveOrder> move_orderer
) :
    board(board), transposition_table(tt), move_orderer(move_orderer)
{}


ChessBoardEvaluator::~ChessBoardEvaluator()
{}


Square ChessBoardEvaluator::__fix_pst_square(Square square)
{
    if (board->GetTurnColor() == TURN_WHITE)
        return square;
    
    uint8_t x = get_piece_x(square);
    uint8_t y = get_piece_y(square);

    return flatten_xy(x, 7 - y);
}


Evaluation ChessBoardEvaluator::EvaluatePieceValues()
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


Evaluation ChessBoardEvaluator::EvaluatePSTs()
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
    bool turn = board->GetTurnColor();

    Evaluation eval_white = 0;
    Evaluation eval_black = 0;

    // ------------------------------------------------------------
    // White evaluation.
    // ------------------------------------------------------------

    board->SetTurnColor(TURN_WHITE);

    eval_white += PIECE_VALUE_MULTIPLIER    * EvaluatePieceValues();
    eval_white += PST_EVAL_MULTIPLIER       * EvaluatePSTs();
    eval_white += MOBILITY_MULTIPLIER       * EvaluateMobility();

    if (board->IsCheck())
        eval_white -= 100;

    // ------------------------------------------------------------
    // Black evaluation.
    // ------------------------------------------------------------

    board->SetTurnColor(TURN_BLACK);

    eval_black += PIECE_VALUE_MULTIPLIER    * EvaluatePieceValues();
    eval_black += PST_EVAL_MULTIPLIER       * EvaluatePSTs();
    eval_black += MOBILITY_MULTIPLIER       * EvaluateMobility();

    if (board->IsCheck())
        eval_black -= 100;

    // Restore original turn.
    board->SetTurnColor(turn);

    Evaluation base = eval_white - eval_black;

    // Mop-up is more useful in the endgame, and is really expensive to calculate.
    if (IsEndgame())
    {
        base +=
            MOP_UP_MULTIPLIER * ComputeMopupBonus();
    }

    return base;
}


Evaluation ChessBoardEvaluator::QuiesenceSearchMain(
    int depth,
    Evaluation alpha,
    Evaluation beta)
{
    // Terminal positions.
    if (board->IsCheckMate())
        return board->GetTurnColor() ? -CHECKMATE_SCORE : CHECKMATE_SCORE;

    if (board->IsStaleMate())
        return 0;

    const bool maximizing =
        board->GetTurnColor() == TURN_WHITE;

    // --------------------------------------------------------
    // If we're in check, we cannot stand-pat.
    // Search all evasions.
    // --------------------------------------------------------

    if (board->IsCheck())
    {
        if (depth <= 0)
            return EvaluatePosition();

        auto moves = board->GetLegalMoves();

        if (moves.empty())
            return maximizing ? -CHECKMATE_SCORE : CHECKMATE_SCORE;

        move_orderer->OrderMoves(moves, 0);

        for (Move& move : moves)
        {
            board->MakeMove(move);

            Evaluation eval =
                QuiesenceSearchMain(
                    depth - 1,
                    alpha,
                    beta);

            board->UndoMove(move);

            if (maximizing)
            {
                alpha = std::max(alpha, eval);

                if (alpha >= beta)
                    break;
            }
            else
            {
                beta = std::min(beta, eval);

                if (alpha >= beta)
                    break;
            }
        }

        return maximizing ? alpha : beta;
    }

    // --------------------------------------------------------
    // Stand pat.
    // --------------------------------------------------------

    Evaluation stand_pat = EvaluatePosition();

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

    // --------------------------------------------------------
    // Depth limit.
    // --------------------------------------------------------

    if (depth <= 0)
        return stand_pat;

    auto captures = board->GetLegalCaptures();

    if (captures.empty())
        return stand_pat;

    move_orderer->OrderMoves(captures, 0);

    // --------------------------------------------------------
    // Search captures.
    // --------------------------------------------------------

    for (int i = 0; i < static_cast<int>(captures.size()); ++i)
    {
        Move move = captures[i];

        board->MakeMove(move);

        Evaluation eval =
            QuiesenceSearchMain(
                depth - 1,
                alpha,
                beta);

        board->UndoMove(move);

        if (maximizing)
        {
            alpha = std::max(alpha, eval);

            if (alpha >= beta)
                break;
        }
        else
        {
            beta = std::min(beta, eval);

            if (alpha >= beta)
                break;
        }
    }

    return maximizing ? alpha : beta;
}


Evaluation ChessBoardEvaluator::QuiesenceSearch(int depth)
{
    Evaluation alpha = INT_MIN;
    Evaluation beta = INT_MAX;

    return QuiesenceSearchMain(depth, alpha, beta);
}

