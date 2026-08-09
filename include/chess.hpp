#pragma once

#include <cstdint>

#include <vector>
#include <string>

// Note: pieces only actually take up 6 bits.
using Square = uint8_t;

#define PIECE_TYPE_NONE     0x00
#define PIECE_TYPE_PAWN     0x01
#define PIECE_TYPE_KNIGHT   0x02
#define PIECE_TYPE_BISHOP   0x03
#define PIECE_TYPE_ROOK     0x04
#define PIECE_TYPE_QUEEN    0x05
#define PIECE_TYPE_KING     0x06

#define PIECE_COLOR_NONE    0x00
#define PIECE_COLOR_WHITE   0x08
#define PIECE_COLOR_BLACK   0x10

#define NULL_PIECE  ( PIECE_TYPE_NONE | PIECE_COLOR_NONE )

#define flatten_xy(x, y)    ( ((y) << 3) | (x) )
#define get_piece_x(square) ( (square) & 0x07 )
#define get_piece_y(square) ( (square) >> 3 )

// Note: pieces only actually take up 5 bits.
using Piece = uint8_t;

struct Move
{
    // To / from squares.
    Square from;
    Square to;

    // Piece moved.
    Piece moved;

    // Piece captured.
    Piece captured;

    // Bits 0, 1, 2: promoted piece (-color).
    // Bit 3: Was the move castling?
    uint8_t flags;
};

// 64 square bitboard.
using Bitboard = uint64_t;

#define TURN_WHITE  0
#define TURN_BLACK  1

// Chess board turn color
using TurnColor = bool;

class ChessBoard
{
    Piece pieces[64];

    // Occupancy bitboard for each piece and white / black.
    Bitboard occupancy_bitboards[32];
    Bitboard occupancy_bitboard_white;
    Bitboard occupancy_bitboard_black;

    // Attack bitboard for each piece and white / black.
    Bitboard attack_bitboards[32];
    Bitboard attack_bitboard_white;
    Bitboard attack_bitboard_black;

    TurnColor turn;

    void UpdateOccupancyBitboards();
    void UpdateAttackBitboards();

    void ComputeAttackLookupBitboards();

    void GetLegalPawnAttacks(std::vector<Move>& moves);
    void GetLegalKnightAttacks(std::vector<Move>& moves);
    void GetLegalBishopAttacks(std::vector<Move>& moves);
    void GetLegalQueenAttacks(std::vector<Move>& moves);
    void GetLegalKingAttacks(std::vector<Move>& moves);

    void GetLegalPawnMoves(std::vector<Move>& moves);
    void GetLegalKnightMoves(std::vector<Move>& moves);
    void GetLegalBishopMoves(std::vector<Move>& moves);
    void GetLegalQueenMoves(std::vector<Move>& moves);
    void GetLegalKingMoves(std::vector<Move>& moves);
    

public:
    ChessBoard();
    ~ChessBoard();

    void MakeMove(Move& move);
    void UndoMove(Move move);

    std::vector<Move> GetLegalMoves();

    bool IsLegalMove(Move move);

    bool IsCheck();
    bool IsCheckMate();
    bool IsStaleMate();

    uint8_t CountPawns();
    uint8_t CountKnights();
    uint8_t CountBishops();
    uint8_t CountRooks();
    uint8_t CountQueens();
    uint8_t CountKings();

    inline bool GetTurnColor()
    { return turn; }

    inline void SetTurnColor(bool turn_color)
    { turn = turn_color; }

    // Load board from FEN string. Returns true on success.
    bool LoadFEN(const std::string& fen);
};
