#include "eval.hpp"
#include <algorithm>


// Piece values
const int PAWN_VALUE    = 100;
const int KNIGHT_VALUE  = 300;
const int BISHOP_VALUE  = 320;
const int ROOK_VALUE    = 500;
const int QUEEN_VALUE   = 900;

// Evaluation multipliers
const float PIECE_VALUE_MULTIPLIER = 1;
const float PST_EVAL_MULTIPLIER    = 1.5;
const float MOP_UP_MULTIPLIER      = 2;


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


ChessBoardEvaluation::ChessBoardEvaluation(std::shared_ptr<ChessBoard> _board) : board(_board)
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
                eval += KING_PST[pst_square];
                break;
            default:
                break;
        }
    }

    return eval;
}


Evaluation ChessBoardEvaluation::ComputeMopupBonus()
{
    // Mop-up bonus: if one side has only a king and the other side has
    // significant material (rook or queen), give a small bonus that
    // grows the closer the attacker's major piece is to the lone king.

    int white_material = 0;
    int black_material = 0;
    int white_nonking_count = 0;
    int black_nonking_count = 0;
    Square white_king_sq = 64;
    Square black_king_sq = 64;
    std::vector<Square> white_attack_sqs;
    std::vector<Square> black_attack_sqs;

    for (Square sq = 0; sq < 64; ++sq)
    {
        Piece p = board->GetPieceAt(sq);
        if (p == NULL_PIECE) continue;
        uint8_t type = p & 0x07;
        bool is_white = bool(p & PIECE_COLOR_WHITE);

        int val = 0;
        switch (type)
        {
            case PIECE_TYPE_PAWN: val = PAWN_VALUE; break;
            case PIECE_TYPE_KNIGHT: val = KNIGHT_VALUE; break;
            case PIECE_TYPE_BISHOP: val = BISHOP_VALUE; break;
            case PIECE_TYPE_ROOK: val = ROOK_VALUE; break;
            case PIECE_TYPE_QUEEN: val = QUEEN_VALUE; break;
            case PIECE_TYPE_KING: val = 0; break;
            default: val = 0; break;
        }

        if (is_white)
        {
            white_material += val;
            if (type != PIECE_TYPE_KING)
            {
                white_nonking_count++;
                if (type == PIECE_TYPE_ROOK || type == PIECE_TYPE_QUEEN)
                    white_attack_sqs.push_back(sq);
            }
            else
                white_king_sq = sq;
        }
        else
        {
            black_material += val;
            if (type != PIECE_TYPE_KING)
            {
                black_nonking_count++;
                if (type == PIECE_TYPE_ROOK || type == PIECE_TYPE_QUEEN)
                    black_attack_sqs.push_back(sq);
            }
            else
                black_king_sq = sq;
        }
    }

    auto chebyshev_dist = [](Square a, Square b) -> int {
        uint8_t ax = get_piece_x(a), ay = get_piece_y(a);
        uint8_t bx = get_piece_x(b), by = get_piece_y(b);
        int dx = (int)ax - (int)bx;
        int dy = (int)ay - (int)by;
        dx = dx < 0 ? -dx : dx;
        dy = dy < 0 ? -dy : dy;
        return dx > dy ? dx : dy;
    };

    float white_bonus = 0.0f;
    float black_bonus = 0.0f;

    // If white has only king and black has at least a rook/queen
    if (white_nonking_count == 0 && black_material >= ROOK_VALUE && white_king_sq < 64 && black_king_sq < 64)
    {
        int min_dist_major = 8;
        for (Square s : black_attack_sqs)
            min_dist_major = std::min(min_dist_major, chebyshev_dist(s, white_king_sq));

        int king_dist = chebyshev_dist(black_king_sq, white_king_sq);
        int effective_dist = std::min(min_dist_major, king_dist);

        float mop_bonus = 180.0f;
        mop_bonus += (black_material - ROOK_VALUE) * 0.05f;
        mop_bonus -= effective_dist * 18.0f; // stronger penalty for distance
        if (mop_bonus < 0) mop_bonus = 0;
        black_bonus += mop_bonus;

        // Bonus for attacker's king proximity to the lone king (encourage using king)
        float king_prox_bonus = std::max(0.0f, 80.0f - (float)king_dist * 12.0f);
        black_bonus += king_prox_bonus;
    }

    // If black has only king and white has at least a rook/queen
    if (black_nonking_count == 0 && white_material >= ROOK_VALUE && white_king_sq < 64 && black_king_sq < 64)
    {
        int min_dist_major = 8;
        for (Square s : white_attack_sqs)
            min_dist_major = std::min(min_dist_major, chebyshev_dist(s, black_king_sq));

        int king_dist = chebyshev_dist(white_king_sq, black_king_sq);
        int effective_dist = std::min(min_dist_major, king_dist);

        float mop_bonus = 180.0f;
        mop_bonus += (white_material - ROOK_VALUE) * 0.05f;
        mop_bonus -= effective_dist * 18.0f;
        if (mop_bonus < 0) mop_bonus = 0;
        white_bonus += mop_bonus;

        float king_prox_bonus = std::max(0.0f, 80.0f - (float)king_dist * 12.0f);
        white_bonus += king_prox_bonus;
    }

    return white_bonus - black_bonus;
}


Evaluation ChessBoardEvaluation::EvaluatePosition()
{
    bool turn = board->GetTurnColor();
    Evaluation eval_white = 0;
    Evaluation eval_black = 0;

    board->SetTurnColor(TURN_WHITE);
    eval_white += PIECE_VALUE_MULTIPLIER * EvaluatePieceValues();
    eval_white += PST_EVAL_MULTIPLIER * EvaluatePSTs();

    board->SetTurnColor(TURN_BLACK);
    eval_black += PIECE_VALUE_MULTIPLIER * EvaluatePieceValues();
    eval_black += PST_EVAL_MULTIPLIER * EvaluatePSTs();

    board->SetTurnColor(turn);

    Evaluation base = eval_white - eval_black;

    base += MOP_UP_MULTIPLIER * ComputeMopupBonus();
    
    return base;
}

