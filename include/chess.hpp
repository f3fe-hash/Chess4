#pragma once

#include <cstdint>

#include <vector>
#include <string>

#define A1 0
#define B1 1
#define C1 2
#define D1 3
#define E1 4
#define F1 5
#define G1 6
#define H1 7
#define A2 8
#define B2 9
#define C2 10
#define D2 11
#define E2 12
#define F2 13
#define G2 14
#define H2 15
#define A3 16
#define B3 17
#define C3 18
#define D3 19
#define E3 20
#define F3 21
#define G3 22
#define H3 23
#define A4 24
#define B4 25
#define C4 26
#define D4 27
#define E4 28
#define F4 29
#define G4 30
#define H4 31
#define A5 32
#define B5 33
#define C5 34
#define D5 35
#define E5 36
#define F5 37
#define G5 38
#define H5 39
#define A6 40
#define B6 41
#define C6 42
#define D6 43
#define E6 44
#define F6 45
#define G6 46
#define H6 47
#define A7 48
#define B7 49
#define C7 50
#define D7 51
#define E7 52
#define F7 53
#define G7 54
#define H7 55
#define A8 56
#define B8 57
#define C8 58
#define D8 59
#define E8 60
#define F8 61
#define G8 62
#define H8 63

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

constexpr uint8_t MOVE_NORMAL             = 0x00;
constexpr uint8_t MOVE_PROMOTION          = 0x01;
constexpr uint8_t MOVE_CASTLE_KINGSIDE    = 0x02;
constexpr uint8_t MOVE_CASTLE_QUEENSIDE   = 0x04;

enum CastlingRights
{
    CASTLE_NONE  = 0,
    CASTLE_WK    = 1 << 0, // White kingside
    CASTLE_WQ    = 1 << 1, // White queenside
    CASTLE_BK    = 1 << 2, // Black kingside
    CASTLE_BQ    = 1 << 3  // Black queenside
};

struct Move
{
    // To / from squares.
    Square from;
    Square to;

    // Piece moved.
    Piece moved;

    // Piece captured.
    Piece captured;

    int8_t flags;
    CastlingRights prev_castling_rights;
};

struct ZobristTable 
{
    // 12 pieces (indices 0-5 White, 6-11 Black) across 64 squares
    uint64_t pieces[12][64];
    // 16 combinations of castling rights
    uint64_t castling[16];
    // Key toggled when it is Black's turn to move
    uint64_t side_to_move;

    ZobristTable() 
    {
        // Simple, predictable LCG to guarantee exact cross-platform values
        uint64_t state = 1804289383; 
        auto next_rand = [&]() mutable -> uint64_t {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            return state * 0x2545F4914F6CDD1DULL;
        };

        for (int p = 0; p < 12; ++p)
            for (int s = 0; s < 64; ++s)
                pieces[p][s] = next_rand();

        for (int c = 0; c < 16; ++c)
            castling[c] = next_rand();

        side_to_move = next_rand();
    }
};

// Global instance of keys initialized once at application startup
inline const ZobristTable zobrist_keys;

// 64 square bitboard.
using Bitboard = uint64_t;

#define TURN_WHITE  1
#define TURN_BLACK  0

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

    CastlingRights castling_rights;

    uint64_t zobrist_hash;

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
    void GetLegalRookMoves(std::vector<Move>& moves);
    void GetLegalQueenMoves(std::vector<Move>& moves);
    void GetLegalKingMoves(std::vector<Move>& moves);

    void AddCastlingMoves(
        std::vector<Move>& moves,
        Square kingSquare,
        Piece king);

    bool IsSquareAttacked(Square square, TurnColor byColor);

    inline int GetZobristPieceIndex(Piece piece) const 
    {
        uint8_t type = piece & 0x07;
        uint8_t color = piece & 0x18;
        int base = (color == PIECE_COLOR_WHITE) ? 0 : 6;
        return base + (type - 1);
    }
    
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

    Square PopPawns();
    Square PopKnights();
    Square PopBishops();
    Square PopRooks();
    Square PopQueens();
    Square PopKings();

    inline bool GetTurnColor()
    { return turn; }

    inline void SetTurnColor(bool turn_color)
    { turn = turn_color; }

    Piece GetPieceAt(Square square) const;

    // Load board from FEN string. Returns true on success.
    bool LoadFEN(const std::string& fen);

    uint64_t GenerateZobristHash() const;

    // Fast, inline getter for the pre-calculated state hash
    inline uint64_t GetZobristHash() const { return zobrist_hash; }
};
