#pragma once

#include <cstdint>

#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <cctype>

#ifdef DEBUG
#include <iostream>
#endif

#include "bitboards.hpp"

#define SQ_A1 0
#define SQ_B1 1
#define SQ_C1 2
#define SQ_D1 3
#define SQ_E1 4
#define SQ_F1 5
#define SQ_G1 6
#define SQ_H1 7
#define SQ_A2 8
#define SQ_B2 9
#define SQ_C2 10
#define SQ_D2 11
#define SQ_E2 12
#define SQ_F2 13
#define SQ_G2 14
#define SQ_H2 15
#define SQ_A3 16
#define SQ_B3 17
#define SQ_C3 18
#define SQ_D3 19
#define SQ_E3 20
#define SQ_F3 21
#define SQ_G3 22
#define SQ_H3 23
#define SQ_A4 24
#define SQ_B4 25
#define SQ_C4 26
#define SQ_D4 27
#define SQ_E4 28
#define SQ_F4 29
#define SQ_G4 30
#define SQ_H4 31
#define SQ_A5 32
#define SQ_B5 33
#define SQ_C5 34
#define SQ_D5 35
#define SQ_E5 36
#define SQ_F5 37
#define SQ_G5 38
#define SQ_H5 39
#define SQ_A6 40
#define SQ_B6 41
#define SQ_C6 42
#define SQ_D6 43
#define SQ_E6 44
#define SQ_F6 45
#define SQ_G6 46
#define SQ_H6 47
#define SQ_A7 48
#define SQ_B7 49
#define SQ_C7 50
#define SQ_D7 51
#define SQ_E7 52
#define SQ_F7 53
#define SQ_G7 54
#define SQ_H7 55
#define SQ_A8 56
#define SQ_B8 57
#define SQ_C8 58
#define SQ_D8 59
#define SQ_E8 60
#define SQ_F8 61
#define SQ_G8 62
#define SQ_H8 63

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

#define NULL_PIECE ( PIECE_TYPE_NONE | PIECE_COLOR_NONE )

#define NULL_SQUARE (64)

#define flatten_xy(x, y)        ( ((y) << 3) | (x) )
#define get_piece_x(square)     ( (square) & 0x07 )
#define get_piece_y(square)     ( (square) >> 3 )
#define get_piece_type(piece)   ( (piece) & 0x07 )
#define get_piece_color(piece)  ( (piece) & 0x18 )

// Note: pieces only actually take up 5 bits.
using Piece = uint8_t;

constexpr uint8_t MOVE_NORMAL             = 0x00;
constexpr uint8_t MOVE_PROMOTION          = 0x01;
constexpr uint8_t MOVE_CASTLE_KINGSIDE    = 0x02;
constexpr uint8_t MOVE_CASTLE_QUEENSIDE   = 0x04;
constexpr uint8_t MOVE_EN_PASSANT         = 0x08;

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
    Square from = NULL_SQUARE;
    Square to = NULL_SQUARE;

    Piece moved = NULL_PIECE;
    Piece captured = NULL_PIECE;

    // Piece a pawn promotes to. NULL_PIECE for normal moves.
    Piece promotion = NULL_PIECE;

    uint8_t flags = MOVE_NORMAL;
    CastlingRights prev_castling_rights = CASTLE_NONE;
    Square prev_en_passant = NULL_SQUARE;

    bool IsCapture() const
    {
        return (captured != NULL_PIECE) | (flags & MOVE_EN_PASSANT);
    }

    bool IsPromotion() const
    {
        return promotion != NULL_PIECE;
    }

    bool IsQuietMove() const
    {
        return (captured == NULL_PIECE) && (promotion == NULL_PIECE);
    }

    bool IsNull() const
    {
        return from == 64;
    }

    std::string ToStr() const
    {
        if (IsNull())
            return "0000";
        
        char buff[5] = {};

        buff[0] = get_piece_x(from) + 'a';
        buff[1] = get_piece_y(from) + '1';
        buff[2] = get_piece_x(to) + 'a';
        buff[3] = get_piece_y(to) + '1';

        return std::string(buff);
    }

    bool operator==(const Move& other) const
    {
        return from == other.from &&
            to == other.to &&
            moved == other.moved &&
            captured == other.captured &&
            promotion == other.promotion;
    }
};

using ZobristHash = uint64_t;

struct ZobristTable 
{
    // 12 pieces (indices 0-5 White, 6-11 Black) across 64 squares
    uint64_t pieces[12][64];
    // 16 combinations of castling rights
    uint64_t castling[16];
    // Key toggled when it is Black's turn to move
    uint64_t side_to_move;
    // One key per en-passant file.
    uint64_t en_passant[8];

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

        for (int file = 0; file < 8; ++file)
            en_passant[file] = next_rand();
    }
};

// Global instance of keys initialized once at application startup
inline const ZobristTable zobrist_keys;

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

    Square en_passant;

    uint16_t halfmove_clock = 0;
    uint16_t fullmove_number = 1;

    uint64_t zobrist_hash;

    std::vector<ZobristHash> history;

    void UpdateOccupancyBitboards();
    void UpdateAttackBitboards();
    void UpdateAttackBitboardsOnly();

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

    constexpr uint64_t GetEnPassantZobristKey(
        const Square en_passant)
    {
        if (en_passant == 64)
            return 0;

        return zobrist_keys.en_passant[
            get_piece_x(en_passant)
        ];
    }

    size_t GetNumMoves() const;
    size_t GetNumCaptures() const;
    
public:
    ChessBoard();
    ~ChessBoard();

    void MakeMove(Move& move);
    void UndoMove(Move move);

    std::vector<Move> GetLegalMoves();
    std::vector<Move> GetLegalCaptures();

    void GetNumLegalMovesAndCaptures(size_t& moves_count, size_t& captures_count) const;

    bool IsLegalMove(Move move);

    bool IsCheck();
    bool IsThreeFoldRepition() const;
    bool IsCheckMate();
    bool IsStaleMate();

    uint8_t CountPawns() const;
    uint8_t CountKnights() const;
    uint8_t CountBishops() const;
    uint8_t CountRooks() const;
    uint8_t CountQueens() const;
    uint8_t CountKings() const;

    Square PopPawns() const;
    Square PopKnights() const;
    Square PopBishops() const;
    Square PopRooks() const;
    Square PopQueens() const;
    Square PopKings() const;

    Bitboard GetPawns() const;
    Bitboard GetKnights() const;
    Bitboard GetBishops() const;
    Bitboard GetRooks() const;
    Bitboard GetQueens() const;
    Bitboard GetKings() const;

    bool HasPseudoLegalCapture() const;

    inline bool GetTurnColor() const
    { return turn; }

    inline void SetTurnColor(bool turn_color)
    { turn = turn_color; }

    Piece GetPieceAt(Square square) const;

    // Load board from FEN string. Returns true on success.
    bool LoadFEN(const std::string& fen);

    std::string GetFEN() const;

    uint64_t GenerateZobristHash() const;

    // Fast, inline getter for the pre-calculated state hash
    inline uint64_t GetZobristHash() const { return zobrist_hash; }
};
