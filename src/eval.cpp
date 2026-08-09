#include "eval.hpp"


// Piece values
const int PAWN_VALUE    = 100;
const int KNIGHT_VALUE  = 300;
const int BISHOP_VALUE  = 320;
const int ROOK_VALUE    = 500;
const int QUEEN_VALUE   = 900;

// Evaluation multipliers
const float PIECE_VALUE_MULTIPLIER = 1;
const float PST_EVAL_MULTIPLIER = 1.5;


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
    return eval_white - eval_black;
}

