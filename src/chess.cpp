#include "chess.hpp"

#include <sstream>

namespace
{

static Bitboard pawn_attack_lookup[2][64];
static Bitboard knight_attack_lookup[64];
static Bitboard king_attack_lookup[64];
static Bitboard bishop_rays[64];
static Bitboard rook_rays[64];
static Bitboard queen_rays[64];
static Bitboard bishop_masks[64];
static Bitboard rook_masks[64];
static Bitboard bishop_magics[64];
static Bitboard rook_magics[64];
static uint8_t bishop_magic_shifts[64];
static uint8_t rook_magic_shifts[64];
static Bitboard bishop_attacks[64][8192];
static Bitboard rook_attacks[64][16384];
static bool attack_lookup_initialized = false;

inline Bitboard SquareMask(const Square square)
{
    return Bitboard(1ULL) << square;
}

inline bool IsInRange7(const uint8_t x)
{
    // (x >= 0) && (x < 8)
    return !bool(x & (~7));
}

inline bool IsOnBoard(const uint8_t x, const uint8_t y)
{
    // GCC will optimize (simplify) the logic operations.
    // return (x >= 0) && (x < 8) && (y >= 0) && (y < 8);
    return IsInRange7(x) && IsInRange7(y);
}

inline Square FlattenSquare(const uint8_t x, const uint8_t y)
{
    return Square(flatten_xy(x, y));
}

inline bool PieceIsFriendly(const Piece piece, const TurnColor color)
{
    if (piece == NULL_PIECE)
        return false;
    
    return (color == TURN_WHITE) ?
        bool(piece & PIECE_COLOR_WHITE) :
        bool(piece & PIECE_COLOR_BLACK);
}

inline bool PieceIsOpponent(const Piece piece, const TurnColor color)
{
    if (piece == NULL_PIECE)
        return false;
    
    return (color == TURN_WHITE) ?
        bool(piece & PIECE_COLOR_BLACK) :
        bool(piece & PIECE_COLOR_WHITE);
}

inline Square PopLeastSignificantBit(Bitboard& bits)
{
    Square sq = Square(__builtin_ctzll(bits));
    bits &= bits - 1;
    return sq;
}

inline Square PopBitboard(Bitboard bits)
{
    if (!bits)
        return Square(64);

    return PopLeastSignificantBit(bits);
}

inline void AddMove(
    std::vector<Move>& moves,
    const Square from,
    const Square to,
    const Piece moved,
    const Piece captured,
    const CastlingRights castlingRights)
{
    const bool promotion =
        get_piece_type(moved) == PIECE_TYPE_PAWN &&
        get_piece_y(to) == (get_piece_color(moved) == PIECE_COLOR_WHITE ? 7 : 0);
    
    const Piece promotionTypes[] = {
        PIECE_TYPE_QUEEN, PIECE_TYPE_ROOK,
        PIECE_TYPE_BISHOP, PIECE_TYPE_KNIGHT
    };

    const int promotionCount = promotion ? 4 : 1;
    for (int index = 0; index < promotionCount; ++index)
    {
        Move move{};
        move.from = from;
        move.to = to;
        move.moved = moved;
        move.captured = captured;
        move.flags = promotion ? MOVE_PROMOTION : MOVE_NORMAL;
        move.prev_castling_rights = castlingRights;
        if (promotion)
            move.promotion = promotionTypes[index] | get_piece_color(moved);
        
        moves.push_back(move);
    }
}

Bitboard ComputeSlidingAttacks(const Square square, const Bitboard occupancy, const int directions[][2], const int directionCount)
{
    Bitboard attacks = 0ULL;
    int startX = get_piece_x(square);
    int startY = get_piece_y(square);

    for (int i = 0; i < directionCount; ++i)
    {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int x = startX + dx;
        int y = startY + dy;

        while (IsOnBoard(x, y))
        {
            Square target = FlattenSquare(x, y);
            Bitboard mask = SquareMask(target);
            attacks |= mask;
            if (occupancy & mask)
                break;
            
            x += dx;
            y += dy;
        }
    }

    return attacks;
}

inline Bitboard NextRandom(Bitboard& state)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return state * 0x2545F4914F6CDD1DULL;
}

inline Bitboard SparseRandom(Bitboard& state)
{
    return NextRandom(state) & NextRandom(state) & NextRandom(state);
}

inline Bitboard OccupancyFromIndex(unsigned index, Bitboard mask)
{
    Bitboard occupancy = 0ULL;
    unsigned bit = 0;
    while (mask)
    {
        Bitboard least = mask & -mask;
        if (index & (1U << bit))
            occupancy |= least;
        mask &= mask - 1;
        ++bit;
    }
    return occupancy;
}

Bitboard FindMagic(
    const Square square,
    const Bitboard mask,
    const int directions[][2],
    const int directionCount,
    const unsigned bits,
    Bitboard& randomState)
{
    const unsigned tableSize = 1U << bits;
    std::vector<Bitboard> occupancies(tableSize);
    std::vector<Bitboard> attacks(tableSize);

    for (unsigned index = 0; index < tableSize; ++index)
    {
        occupancies[index] = OccupancyFromIndex(index, mask);
        attacks[index] = ComputeSlidingAttacks(
            square, occupancies[index], directions, directionCount);
    }

    for (;;)
    {
        const Bitboard magic = SparseRandom(randomState);
        if (std::popcount((mask * magic) & 0xFF00000000000000ULL) < 6)
            continue;

        std::vector<Bitboard> used(tableSize, 0ULL);
        std::vector<bool> occupied(tableSize, false);
        bool valid = true;

        for (unsigned index = 0; index < tableSize; ++index)
        {
            const unsigned magicIndex = static_cast<unsigned>(
                (occupancies[index] * magic) >> (64 - bits));

            if (occupied[magicIndex])
            {
                if (used[magicIndex] != attacks[index])
                {
                    valid = false;
                    break;
                }
            }
            else
            {
                occupied[magicIndex] = true;
                used[magicIndex] = attacks[index];
            }
        }

        if (valid)
            return magic;
    }
}

inline Bitboard SlidingAttacks(const Square square, const Bitboard occupancy, const bool bishop)
{
    const Bitboard mask = bishop ? bishop_masks[square] : rook_masks[square];
    const Bitboard magic = bishop
        ? bishop_magics[square]
        : rook_magics[square];
    const unsigned shift = bishop
        ? bishop_magic_shifts[square]
        : rook_magic_shifts[square];
    const unsigned index = static_cast<unsigned>(
        ((occupancy & mask) * magic) >> shift);
    return bishop ? bishop_attacks[square][index] : rook_attacks[square][index];
}

constexpr Bitboard WHITE_KINGSIDE_EMPTY  =
    (1ULL << F1) | (1ULL << G1);

constexpr Bitboard WHITE_QUEENSIDE_EMPTY =
    (1ULL << B1) | (1ULL << C1) | (1ULL << D1);

constexpr Bitboard BLACK_KINGSIDE_EMPTY  =
    (1ULL << F8) | (1ULL << G8);

constexpr Bitboard BLACK_QUEENSIDE_EMPTY =
    (1ULL << B8) | (1ULL << C8) | (1ULL << D8);

}


ChessBoard::ChessBoard()
{
    ComputeAttackLookupBitboards();

    for (int i = 0; i < 64; ++i)
        pieces[i] = NULL_PIECE;

    for (int i = 0; i < 32; ++i)
    {
        occupancy_bitboards[i] = 0ULL;
        attack_bitboards[i] = 0ULL;
    }

    occupancy_bitboard_white = 0ULL;
    occupancy_bitboard_black = 0ULL;
    attack_bitboard_white = 0ULL;
    attack_bitboard_black = 0ULL;
    turn = TURN_WHITE;

    castling_rights = CastlingRights(CASTLE_WK | CASTLE_WQ | CASTLE_BK | CASTLE_BQ);
    en_passant = 64;
}


ChessBoard::~ChessBoard()
{}


ZobristHash ChessBoard::GenerateZobristHash() const
{
    ZobristHash hash = 0;
    for (int s = 0; s < 64; ++s)
    {
        if (pieces[s] != NULL_PIECE)
        {
            hash ^= zobrist_keys.pieces[GetZobristPieceIndex(pieces[s])][s];
        }
    }

    hash ^= zobrist_keys.castling[castling_rights];
    if (en_passant < 64)
        hash ^= zobrist_keys.en_passant[get_piece_x(en_passant)];

    if (turn == TURN_BLACK)
    {
        hash ^= zobrist_keys.side_to_move;
    }

    return hash;
}


Piece ChessBoard::GetPieceAt(Square square) const
{
    if (square < 64)
        return pieces[square];
    
    return NULL_PIECE;
}


bool ChessBoard::LoadFEN(const std::string& fen)
{
    // Reset position state.
    for (int i = 0; i < 64; ++i)
        pieces[i] = NULL_PIECE;

    castling_rights = CASTLE_NONE;
    en_passant = 64;
    turn = TURN_WHITE;

    // Reset/rebuild Zobrist hash.
    zobrist_hash = 0;

    history.clear();

    // Split FEN by spaces
    std::string part;
    size_t idx = 0;
    size_t len = fen.size();

    // Piece placement
    int rank = 7;
    int file = 0;

    while (idx < len && fen[idx] != ' ')
    {
        char c = fen[idx];
        if (c == '/')
        {
            ++idx;
            --rank;
            file = 0;
            continue;
        }

        if (c >= '1' && c <= '8')
        {
            file += c - '0';
            ++idx;
            continue;
        }

        Piece p = NULL_PIECE;
        bool white = (c >= 'A' && c <= 'Z');
        char lower = (white ? (c - 'A' + 'a') : c);

        switch (lower)
        {
            case 'p': p = PIECE_TYPE_PAWN; break;
            case 'n': p = PIECE_TYPE_KNIGHT; break;
            case 'b': p = PIECE_TYPE_BISHOP; break;
            case 'r': p = PIECE_TYPE_ROOK; break;
            case 'q': p = PIECE_TYPE_QUEEN; break;
            case 'k': p = PIECE_TYPE_KING; break;
            default: p = NULL_PIECE; break;
        }

        if (p != NULL_PIECE)
        {
            if (white)
                p |= PIECE_COLOR_WHITE;
            else
                p |= PIECE_COLOR_BLACK;

            if (rank >= 0 && rank < 8 && file >= 0 && file < 8)
            {
                int square = flatten_xy(file, rank);
                pieces[square] = p;
            }

            ++file;
        }

        ++idx;
    }

    // Skip space
    while (idx < len && fen[idx] == ' ') ++idx;

    // Active color
    if (idx < len)
    {
        char c = fen[idx];
        if (c == 'w')
            turn = TURN_WHITE;
        else if (c == 'b')
            turn = TURN_BLACK;
    }

    std::istringstream fields(fen);
    std::string placement;
    std::string activeColor;
    std::string castling;
    std::string enPassant;
    fields >> placement >> activeColor >> castling >> enPassant;

    if (castling != "-")
    {
        for (char right : castling)
        {
            if (right == 'K') castling_rights = CastlingRights(castling_rights | CASTLE_WK);
            if (right == 'Q') castling_rights = CastlingRights(castling_rights | CASTLE_WQ);
            if (right == 'k') castling_rights = CastlingRights(castling_rights | CASTLE_BK);
            if (right == 'q') castling_rights = CastlingRights(castling_rights | CASTLE_BQ);
        }
    }

    if (enPassant.size() == 2 &&
        enPassant[0] >= 'a' && enPassant[0] <= 'h' &&
        enPassant[1] >= '1' && enPassant[1] <= '8')
    {
        en_passant = FlattenSquare(enPassant[0] - 'a', enPassant[1] - '1');
    }

    // Update occupancies and attacks
    UpdateAttackBitboards();

    zobrist_hash = GenerateZobristHash();
    history.push_back(zobrist_hash);

    return true;
}


void ChessBoard::UpdateOccupancyBitboards()
{
    Bitboard occupancy_white = 0ULL;
    Bitboard occupancy_black = 0ULL;

    for (int i = 0; i < 32; ++i)
        occupancy_bitboards[i] = 0ULL;

    for (Square square = 0; square < 64; ++square)
    {
        Piece piece = pieces[square];
        if (piece == NULL_PIECE)
            continue;

        Bitboard mask = SquareMask(square);
        occupancy_bitboards[piece] |= mask;

        if (piece & PIECE_COLOR_WHITE)
            occupancy_white |= mask;
        else if (piece & PIECE_COLOR_BLACK)
            occupancy_black |= mask;
    }

    occupancy_bitboard_white = occupancy_white;
    occupancy_bitboard_black = occupancy_black;
}


// Updates all attack bitboards.
void ChessBoard::UpdateAttackBitboards()
{
    ComputeAttackLookupBitboards();
    UpdateOccupancyBitboards();

    UpdateAttackBitboardsOnly();
}


void ChessBoard::UpdateAttackBitboardsOnly()
{
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;

    attack_bitboard_white = 0ULL;
    attack_bitboard_black = 0ULL;

    for (int i = 0; i < 32; ++i)
        attack_bitboards[i] = 0ULL;

    const auto addAttacks = [&](const Piece piece)
    {
        Bitboard pieces = occupancy_bitboards[piece];
        while (pieces)
        {
            const Square square = PopLeastSignificantBit(pieces);
            Bitboard attacks = 0ULL;

            switch (get_piece_type(piece))
            {
                case PIECE_TYPE_PAWN:
                    attacks = pawn_attack_lookup[
                        (piece & PIECE_COLOR_WHITE) == 0][square];
                    break;

                case PIECE_TYPE_KNIGHT:
                    attacks = knight_attack_lookup[square];
                    break;

                case PIECE_TYPE_BISHOP:
                    attacks = SlidingAttacks(square, occupancy, true);
                    break;

                case PIECE_TYPE_ROOK:
                    attacks = SlidingAttacks(square, occupancy, false);
                    break;

                case PIECE_TYPE_QUEEN:
                    attacks = SlidingAttacks(square, occupancy, true) |
                        SlidingAttacks(square, occupancy, false);
                    break;

                case PIECE_TYPE_KING:
                    attacks = king_attack_lookup[square];
                    break;

                default:
                    break;
            }

            attack_bitboards[piece] |= attacks;
            if (piece & PIECE_COLOR_WHITE)
                attack_bitboard_white |= attacks;
            else
                attack_bitboard_black |= attacks;
        }
    };

    for (int color : { PIECE_COLOR_WHITE, PIECE_COLOR_BLACK })
    {
        addAttacks(PIECE_TYPE_PAWN | color);
        addAttacks(PIECE_TYPE_KNIGHT | color);
        addAttacks(PIECE_TYPE_BISHOP | color);
        addAttacks(PIECE_TYPE_ROOK | color);
        addAttacks(PIECE_TYPE_QUEEN | color);
        addAttacks(PIECE_TYPE_KING | color);
    }
}


// Computes attack lookup bitboards for all pieces, so generating
// attacks becomes looking in an array, and seeing where pieces can
// actually move.
void ChessBoard::ComputeAttackLookupBitboards()
{
    // Very expensive function
    if (attack_lookup_initialized)
        return;

    constexpr int knightOffsets[8][2] = {
        {1, 2}, {2, 1}, {2, -1}, {1, -2},
        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
    };

    constexpr int kingOffsets[8][2] = {
        {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
    };

    constexpr int bishopDirs[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    constexpr int rookDirs[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    Bitboard randomState = 1804289383ULL;

    for (Square square = 0; square < 64; ++square)
    {
        int x = get_piece_x(square);
        int y = get_piece_y(square);

        Bitboard pawnWhite = 0ULL;
        Bitboard pawnBlack = 0ULL;
        Bitboard knight = 0ULL;
        Bitboard king = 0ULL;
        Bitboard bishop = 0ULL;
        Bitboard rook = 0ULL;

        // Pawn captures
        if (IsOnBoard(x - 1, y + 1))
            pawnWhite |= SquareMask(FlattenSquare(x - 1, y + 1));
        if (IsOnBoard(x + 1, y + 1))
            pawnWhite |= SquareMask(FlattenSquare(x + 1, y + 1));
        if (IsOnBoard(x - 1, y - 1))
            pawnBlack |= SquareMask(FlattenSquare(x - 1, y - 1));
        if (IsOnBoard(x + 1, y - 1))
            pawnBlack |= SquareMask(FlattenSquare(x + 1, y - 1));

        // Knight moves
        for (int i = 0; i < 8; ++i)
        {
            int nx = x + knightOffsets[i][0];
            int ny = y + knightOffsets[i][1];
            if (!IsOnBoard(nx, ny))
                continue;
            knight |= SquareMask(FlattenSquare(nx, ny));
        }

        // King moves
        for (int i = 0; i < 8; ++i)
        {
            int nx = x + kingOffsets[i][0];
            int ny = y + kingOffsets[i][1];
            if (!IsOnBoard(nx, ny))
                continue;
            king |= SquareMask(FlattenSquare(nx, ny));
        }

        // Bishop rays
        for (int i = 0; i < 4; ++i)
        {
            int dx = bishopDirs[i][0];
            int dy = bishopDirs[i][1];
            int cx = x + dx;
            int cy = y + dy;
            while (IsOnBoard(cx, cy))
            {
                bishop |= SquareMask(FlattenSquare(cx, cy));
                cx += dx;
                cy += dy;
            }
        }

        // Rook rays
        for (int i = 0; i < 4; ++i)
        {
            int dx = rookDirs[i][0];
            int dy = rookDirs[i][1];
            int cx = x + dx;
            int cy = y + dy;
            while (IsOnBoard(cx, cy))
            {
                rook |= SquareMask(FlattenSquare(cx, cy));
                cx += dx;
                cy += dy;
            }
        }

        pawn_attack_lookup[0][square] = pawnWhite;
        pawn_attack_lookup[1][square] = pawnBlack;
        knight_attack_lookup[square] = knight;
        king_attack_lookup[square] = king;
        bishop_rays[square] = bishop;
        rook_rays[square] = rook;
        queen_rays[square] = bishop | rook;

        const auto relevantMask = [&](const int directions[][2], const int directionCount) {
            Bitboard mask = 0ULL;
            for (int i = 0; i < directionCount; ++i)
            {
                const int dx = directions[i][0];
                const int dy = directions[i][1];
                int cx = x + dx;
                int cy = y + dy;

                while (IsOnBoard(cx, cy) && IsOnBoard(cx + dx, cy + dy))
                {
                    mask |= SquareMask(FlattenSquare(cx, cy));
                    cx += dx;
                    cy += dy;
                }
            }
            return mask;
        };

        bishop_masks[square] = relevantMask(bishopDirs, 4);
        rook_masks[square] = relevantMask(rookDirs, 4);

        const unsigned bishopBits = std::popcount(bishop_masks[square]);
        const unsigned rookBits = std::popcount(rook_masks[square]);
        bishop_magic_shifts[square] = 64 - bishopBits;
        rook_magic_shifts[square] = 64 - rookBits;
        bishop_magics[square] = FindMagic(
            square, bishop_masks[square], bishopDirs, 4,
            bishopBits, randomState);
        rook_magics[square] = FindMagic(
            square, rook_masks[square], rookDirs, 4,
            rookBits, randomState);

        const unsigned bishopSize = 1U << std::popcount(bishop_masks[square]);
        const unsigned rookSize = 1U << std::popcount(rook_masks[square]);
        for (unsigned index = 0; index < bishopSize; ++index)
        {
            const Bitboard subset = OccupancyFromIndex(
                index, bishop_masks[square]);
            const unsigned magicIndex = static_cast<unsigned>(
                (subset * bishop_magics[square]) >> bishop_magic_shifts[square]);
            bishop_attacks[square][magicIndex] = ComputeSlidingAttacks(
                square, subset, bishopDirs, 4);
        }
        for (unsigned index = 0; index < rookSize; ++index)
        {
            const Bitboard subset = OccupancyFromIndex(
                index, rook_masks[square]);
            const unsigned magicIndex = static_cast<unsigned>(
                (subset * rook_magics[square]) >> rook_magic_shifts[square]);
            rook_attacks[square][magicIndex] = ComputeSlidingAttacks(
                square, subset, rookDirs, 4);
        }
    }

    attack_lookup_initialized = true;
}


void ChessBoard::MakeMove(Move& move)
{
    bool movingWhite = (turn == TURN_WHITE);

    if (move.moved == NULL_PIECE)
        move.moved = pieces[move.from];

    if (move.captured == NULL_PIECE)
        move.captured = pieces[move.to];

    move.prev_castling_rights = castling_rights;
    move.prev_en_passant = en_passant;

    Piece originalPiece = move.moved;
    Piece movedPiece = originalPiece;
    Piece capturedPiece = move.captured;

    // ------------------------------------------------------------
    // Update castling rights before modifying the board.
    // ------------------------------------------------------------

    if ((originalPiece & 0x07) == PIECE_TYPE_KING)
    {
        if (movingWhite)
            castling_rights =
                CastlingRights(castling_rights &
                ~(CASTLE_WK | CASTLE_WQ));
        else
            castling_rights =
                CastlingRights(castling_rights &
                ~(CASTLE_BK | CASTLE_BQ));
    }

    if ((originalPiece & 0x07) == PIECE_TYPE_ROOK)
    {
        if (move.from == H1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WK);

        else if (move.from == A1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WQ);

        else if (move.from == H8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BK);

        else if (move.from == A8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BQ);
    }

    // A rook being captured also removes its castling right.
    if ((capturedPiece & 0x07) == PIECE_TYPE_ROOK)
    {
        if (move.to == H1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WK);

        else if (move.to == A1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WQ);

        else if (move.to == H8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BK);

        else if (move.to == A8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BQ);
    }

    // ------------------------------------------------------------
    // Flag updates
    // ------------------------------------------------------------

    move.flags = MOVE_NORMAL;

    if ((originalPiece & 0x07) == PIECE_TYPE_KING)
    {
        if (movingWhite)
        {
            if (move.from == E1 && move.to == G1)
                move.flags = MOVE_CASTLE_KINGSIDE;
            else if (move.from == E1 && move.to == C1)
                move.flags = MOVE_CASTLE_QUEENSIDE;
        }
        else
        {
            if (move.from == E8 && move.to == G8)
                move.flags = MOVE_CASTLE_KINGSIDE;
            else if (move.from == E8 && move.to == C8)
                move.flags = MOVE_CASTLE_QUEENSIDE;
        }
    }

    if ((originalPiece & 0x07) == PIECE_TYPE_PAWN)
    {
        int fromY = get_piece_y(move.from);
        int toY = get_piece_y(move.to);

        if (move.to == en_passant && move.from != move.to &&
            pieces[move.to] == NULL_PIECE)
        {
            move.flags = MOVE_EN_PASSANT;
            Square capturedSquare = FlattenSquare(
                get_piece_x(move.to), get_piece_y(move.from));
            capturedPiece = pieces[capturedSquare];
            move.captured = capturedPiece;
        }

        if ((movingWhite && fromY == 6 && toY == 7) ||
            (!movingWhite && fromY == 1 && toY == 0))
        {
            move.flags = MOVE_PROMOTION;

            // If the move explicitly specifies a promotion piece,
            // use it. Otherwise retain the old behavior of promoting
            // to a queen.
            if (move.promotion != NULL_PIECE)
            {
                movedPiece = move.promotion;
            }
            else
            {
                move.promotion =
                    PIECE_TYPE_QUEEN |
                    (movingWhite
                        ? PIECE_COLOR_WHITE
                        : PIECE_COLOR_BLACK);

                movedPiece = move.promotion;
            }
        }
    }

    // ------------------------------------------------------------
    // Remove original piece from its old square.
    // ------------------------------------------------------------

    pieces[move.from] = NULL_PIECE;

    occupancy_bitboards[originalPiece] &=
        ~SquareMask(move.from);

    if (movingWhite)
        occupancy_bitboard_white &=
            ~SquareMask(move.from);
    else
        occupancy_bitboard_black &=
            ~SquareMask(move.from);

    // ------------------------------------------------------------
    // Remove captured piece.
    // ------------------------------------------------------------

    if (capturedPiece != NULL_PIECE)
    {
        Square capturedSquare = move.to;
        if (move.flags & MOVE_EN_PASSANT)
            capturedSquare = FlattenSquare(
                get_piece_x(move.to), get_piece_y(move.from));

        pieces[capturedSquare] = NULL_PIECE;
        occupancy_bitboards[capturedPiece] &=
            ~SquareMask(capturedSquare);

        if (capturedPiece & PIECE_COLOR_WHITE)
            occupancy_bitboard_white &=
                ~SquareMask(capturedSquare);
        else
            occupancy_bitboard_black &=
                ~SquareMask(capturedSquare);
    }

    // ------------------------------------------------------------
    // Put moved piece on destination.
    // ------------------------------------------------------------

    pieces[move.to] = movedPiece;

    occupancy_bitboards[movedPiece] |= SquareMask(move.to);

    if (movingWhite)
        occupancy_bitboard_white |= SquareMask(move.to);
    else
        occupancy_bitboard_black |= SquareMask(move.to);

    // ------------------------------------------------------------
    // Castling: move the rook.
    // ------------------------------------------------------------

    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        Square rookFrom = movingWhite ? H1 : H8;
        Square rookTo   = movingWhite ? F1 : F8;

        Piece rook = pieces[rookFrom];

        pieces[rookFrom] = NULL_PIECE;
        pieces[rookTo] = rook;

        occupancy_bitboards[rook] &= ~SquareMask(rookFrom);
        occupancy_bitboards[rook] |= SquareMask(rookTo);

        if (movingWhite)
        {
            occupancy_bitboard_white &= ~SquareMask(rookFrom);
            occupancy_bitboard_white |= SquareMask(rookTo);
        }
        else
        {
            occupancy_bitboard_black &= ~SquareMask(rookFrom);
            occupancy_bitboard_black |= SquareMask(rookTo);
        }
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        Square rookFrom = movingWhite ? A1 : A8;
        Square rookTo   = movingWhite ? D1 : D8;

        Piece rook = pieces[rookFrom];

        pieces[rookFrom] = NULL_PIECE;
        pieces[rookTo] = rook;

        occupancy_bitboards[rook] &= ~SquareMask(rookFrom);
        occupancy_bitboards[rook] |= SquareMask(rookTo);

        if (movingWhite)
        {
            occupancy_bitboard_white &= ~SquareMask(rookFrom);
            occupancy_bitboard_white |= SquareMask(rookTo);
        }
        else
        {
            occupancy_bitboard_black &= ~SquareMask(rookFrom);
            occupancy_bitboard_black |= SquareMask(rookTo);
        }
    }

    en_passant = 64;
    if ((originalPiece & 0x07) == PIECE_TYPE_PAWN &&
        std::abs(int(get_piece_y(move.to)) - int(get_piece_y(move.from))) == 2)
    {
        en_passant = FlattenSquare(
            get_piece_x(move.from),
            (get_piece_y(move.from) + get_piece_y(move.to)) / 2);
    }

    // ------------------------------------------------------------
    // Zobrist: Update zobrist hash value.
    // ------------------------------------------------------------

    // Toggle the turn modifier
    zobrist_hash ^= zobrist_keys.side_to_move;

    // Update changed castling rights (only if they actually changed)
    if (move.prev_castling_rights != castling_rights)
    {
        zobrist_hash ^= zobrist_keys.castling[move.prev_castling_rights];
        zobrist_hash ^= zobrist_keys.castling[castling_rights];
    }

    // Remove the original moving piece from its starting square
    zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(originalPiece)][move.from];

    // Place the FINAL moved piece onto the destination square (Handles Promotion correctly)
    zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(movedPiece)][move.to];

    // Remove a captured piece if present
    if (capturedPiece != NULL_PIECE)
    {
        zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(capturedPiece)][move.to];
    }

    // Update the Rook's position in the hash.
    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        Square rookFrom = movingWhite ? H1 : H8;
        Square rookTo   = movingWhite ? F1 : F8;
        Piece rook = pieces[rookTo]; // The rook is already at rookTo now
        zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(rook)][rookFrom];
        zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(rook)][rookTo];
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        Square rookFrom = movingWhite ? A1 : A8;
        Square rookTo   = movingWhite ? D1 : D8;
        Piece rook = pieces[rookTo];
        zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(rook)][rookFrom];
        zobrist_hash ^= zobrist_keys.pieces[GetZobristPieceIndex(rook)][rookTo];
    }

    move.moved = movedPiece;
    move.captured = capturedPiece;

    turn ^= 1;

    UpdateAttackBitboardsOnly();
    zobrist_hash = GenerateZobristHash();

    history.push_back(zobrist_hash);
}


void ChessBoard::UndoMove(Move move)
{
    bool movingWhite = (turn == TURN_BLACK);

    Piece movedPiece = move.moved;

    // If the move was a promotion, restore a pawn.
    Piece restoredPiece = movedPiece;
    if (move.flags & MOVE_PROMOTION)
    {
        restoredPiece =
            PIECE_TYPE_PAWN |
            (movingWhite
                ? PIECE_COLOR_WHITE
                : PIECE_COLOR_BLACK);
    }

    // ------------------------------------------------------------
    // Remove moved piece from destination.
    // ------------------------------------------------------------

    pieces[move.to] = NULL_PIECE;

    occupancy_bitboards[movedPiece] &=
        ~SquareMask(move.to);

    if (movingWhite)
        occupancy_bitboard_white &=
            ~SquareMask(move.to);
    else
        occupancy_bitboard_black &=
            ~SquareMask(move.to);

    // ------------------------------------------------------------
    // Restore captured piece.
    // ------------------------------------------------------------

    if (move.captured != NULL_PIECE)
    {
        Square capturedSquare = move.to;
        if (move.flags & MOVE_EN_PASSANT)
            capturedSquare = FlattenSquare(
                get_piece_x(move.to), get_piece_y(move.from));
        pieces[capturedSquare] = move.captured;

        occupancy_bitboards[move.captured] |=
            SquareMask(capturedSquare);

        if (move.captured & PIECE_COLOR_WHITE)
            occupancy_bitboard_white |=
                SquareMask(capturedSquare);
        else
            occupancy_bitboard_black |=
                SquareMask(capturedSquare);
    }

    // ------------------------------------------------------------
    // Restore original piece.
    // ------------------------------------------------------------

    pieces[move.from] = restoredPiece;

    occupancy_bitboards[restoredPiece] |=
        SquareMask(move.from);

    if (movingWhite)
        occupancy_bitboard_white |= SquareMask(move.from);
    else
        occupancy_bitboard_black |= SquareMask(move.from);

    // ------------------------------------------------------------
    // Undo castling rook movement.
    // ------------------------------------------------------------

    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        Square rookFrom;
        Square rookTo;

        if (movingWhite)
        {
            rookFrom = H1;
            rookTo = F1;
        }
        else
        {
            rookFrom = H8;
            rookTo = F8;
        }

        Piece rook = pieces[rookTo];

        pieces[rookTo] = NULL_PIECE;
        pieces[rookFrom] = rook;

        occupancy_bitboards[rook] &=
            ~SquareMask(rookTo);

        occupancy_bitboards[rook] |=
            SquareMask(rookFrom);

        if (movingWhite)
        {
            occupancy_bitboard_white &=
                ~SquareMask(rookTo);
            occupancy_bitboard_white |=
                SquareMask(rookFrom);
        }
        else
        {
            occupancy_bitboard_black &=
                ~SquareMask(rookTo);
            occupancy_bitboard_black |=
                SquareMask(rookFrom);
        }
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        Square rookFrom;
        Square rookTo;

        if (movingWhite)
        {
            rookFrom = A1;
            rookTo = D1;
        }
        else
        {
            rookFrom = A8;
            rookTo = D8;
        }

        Piece rook = pieces[rookTo];

        pieces[rookTo] = NULL_PIECE;
        pieces[rookFrom] = rook;

        occupancy_bitboards[rook] &=
            ~SquareMask(rookTo);

        occupancy_bitboards[rook] |=
            SquareMask(rookFrom);

        if (movingWhite)
        {
            occupancy_bitboard_white &=
                ~SquareMask(rookTo);
            occupancy_bitboard_white |=
                SquareMask(rookFrom);
        }
        else
        {
            occupancy_bitboard_black &=
                ~SquareMask(rookTo);
            occupancy_bitboard_black |=
                SquareMask(rookFrom);
        }
    }

    // ------------------------------------------------------------
    // Zobrist: Update zobrist hash value.
    // ------------------------------------------------------------

    // Update turn color modifier
    zobrist_hash ^= zobrist_keys.side_to_move;

    // Update changed castling rights
    zobrist_hash ^= zobrist_keys.castling[move.prev_castling_rights];
    zobrist_hash ^= zobrist_keys.castling[castling_rights];

    // Remove the final piece from the destination square.
    zobrist_hash ^=
        zobrist_keys.pieces[
            GetZobristPieceIndex(move.moved)
        ][move.to];

    // Restore the original piece on the source square.
    Piece originalPiece = move.moved;
    if (move.flags & MOVE_PROMOTION)
    {
        originalPiece =
            PIECE_TYPE_PAWN |
            (movingWhite
                ? PIECE_COLOR_WHITE
                : PIECE_COLOR_BLACK);
    }

    zobrist_hash ^=
        zobrist_keys.pieces[
            GetZobristPieceIndex(originalPiece)
        ][move.from];

    // Remove a captured piece if present
    if (move.captured != NULL_PIECE)
    {
        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(move.captured)
            ][move.to];
    }

    // Restore castling rights exactly as they were.
    castling_rights = move.prev_castling_rights;
    en_passant = move.prev_en_passant;

    turn ^= 1;

    UpdateAttackBitboardsOnly();
    zobrist_hash = GenerateZobristHash();

    (void) history.pop_back();
}


void ChessBoard::GetLegalPawnAttacks(std::vector<Move>& moves)
{
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;
    int pawnIndex = (turn == TURN_WHITE) ? 0 : 1;

    Bitboard pawns = occupancy_bitboards[
            PIECE_TYPE_PAWN |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    
    Square square;

    while (pawns)
    {
        square = PopLeastSignificantBit(pawns);

        Piece piece = pieces[square];
        if ((piece & 0x07) != PIECE_TYPE_PAWN)
            continue;
        if (!PieceIsFriendly(piece, turn))
            continue;

        Bitboard targets = pawn_attack_lookup[pawnIndex][square] & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalKnightAttacks(std::vector<Move>& moves)
{
    
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;
    Bitboard knights = occupancy_bitboards[
        PIECE_TYPE_KNIGHT | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (knights)
    {
        Square square = PopLeastSignificantBit(knights);
        Piece piece = pieces[square];
        Bitboard targets = knight_attack_lookup[square] & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalBishopAttacks(std::vector<Move>& moves)
{
    
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;

    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard bishops = occupancy_bitboards[
        PIECE_TYPE_BISHOP | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (bishops)
    {
        Square square = PopLeastSignificantBit(bishops);
        Piece piece = pieces[square];
        Bitboard targets = SlidingAttacks(square, occupancy, true) & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalQueenAttacks(std::vector<Move>& moves)
{
    
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;

    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard queens = occupancy_bitboards[
        PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (queens)
    {
        Square square = PopLeastSignificantBit(queens);
        Piece piece = pieces[square];
        Bitboard targets = (SlidingAttacks(square, occupancy, true) |
            SlidingAttacks(square, occupancy, false)) & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalKingAttacks(std::vector<Move>& moves)
{
    
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;
    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (kings)
    {
        Square square = PopLeastSignificantBit(kings);
        Piece piece = pieces[square];
        Bitboard targets = king_attack_lookup[square] & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


bool ChessBoard::IsSquareAttacked(Square square, TurnColor byColor)
{
    //Bitboard occupancy =
    //    occupancy_bitboard_white |
    //    occupancy_bitboard_black;

    //Bitboard attackers;

    // Pawns
    Bitboard pawns = occupancy_bitboards[
        PIECE_TYPE_PAWN |
        (byColor == TURN_WHITE
            ? PIECE_COLOR_WHITE
            : PIECE_COLOR_BLACK)
    ];

    while (pawns)
    {
        Square pawnSquare = PopLeastSignificantBit(pawns);

        int pawnIndex = (byColor == TURN_WHITE) ? 0 : 1;

        if (pawn_attack_lookup[pawnIndex][pawnSquare] &
            SquareMask(square))
        {
            return true;
        }
    }

    // Knights
    Bitboard knights = occupancy_bitboards[
        PIECE_TYPE_KNIGHT |
        (byColor == TURN_WHITE
            ? PIECE_COLOR_WHITE
            : PIECE_COLOR_BLACK)
    ];

    while (knights)
    {
        Square knightSquare = PopLeastSignificantBit(knights);

        if (knight_attack_lookup[knightSquare] &
            SquareMask(square))
        {
            return true;
        }
    }

    // Kings
    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING |
        (byColor == TURN_WHITE
            ? PIECE_COLOR_WHITE
            : PIECE_COLOR_BLACK)
    ];

    while (kings)
    {
        Square kingSquare = PopLeastSignificantBit(kings);

        if (king_attack_lookup[kingSquare] &
            SquareMask(square))
        {
            return true;
        }
    }

    // Sliding pieces
    static const int bishopDirs[4][2] =
    {
        { 1,  1},
        { 1, -1},
        {-1,  1},
        {-1, -1}
    };

    static const int rookDirs[4][2] =
    {
        { 1,  0},
        {-1,  0},
        { 0,  1},
        { 0, -1}
    };

    int x = get_piece_x(square);
    int y = get_piece_y(square);

    // Bishops / Queens
    for (int i = 0; i < 4; ++i)
    {
        int cx = x + bishopDirs[i][0];
        int cy = y + bishopDirs[i][1];

        while (IsOnBoard(cx, cy))
        {
            Square target = FlattenSquare(cx, cy);
            Piece piece = pieces[target];

            if (piece != NULL_PIECE)
            {
                if (PieceIsFriendly(piece, byColor))
                {
                    Piece type = Piece(piece & 0x07);

                    if (type == PIECE_TYPE_BISHOP ||
                        type == PIECE_TYPE_QUEEN)
                    {
                        return true;
                    }
                }

                break;
            }

            cx += bishopDirs[i][0];
            cy += bishopDirs[i][1];
        }
    }

    // Rooks / Queens
    for (int i = 0; i < 4; ++i)
    {
        int cx = x + rookDirs[i][0];
        int cy = y + rookDirs[i][1];

        while (IsOnBoard(cx, cy))
        {
            Square target = FlattenSquare(cx, cy);
            Piece piece = pieces[target];

            if (piece != NULL_PIECE)
            {
                if (PieceIsFriendly(piece, byColor))
                {
                    Piece type = Piece(piece & 0x07);

                    if (type == PIECE_TYPE_ROOK ||
                        type == PIECE_TYPE_QUEEN)
                    {
                        return true;
                    }
                }

                break;
            }

            cx += rookDirs[i][0];
            cy += rookDirs[i][1];
        }
    }

    return false;
}


void ChessBoard::GetLegalPawnMoves(std::vector<Move>& moves)
{
    
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;

    int forward = (turn == TURN_WHITE) ? 1 : -1;
    int startRank = (turn == TURN_WHITE) ? 1 : 6;

    Bitboard pawns = occupancy_bitboards[
        PIECE_TYPE_PAWN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (pawns)
    {
        Square square = PopLeastSignificantBit(pawns);
        Piece piece = pieces[square];

        int x = get_piece_x(square);
        int y = get_piece_y(square);
        int targetY = y + forward;

        if (!IsOnBoard(x, targetY))
            continue;

        Square singleTarget = FlattenSquare(x, targetY);
        Bitboard singleMask = SquareMask(singleTarget);
        if (!(occupancy & singleMask))
        {
            AddMove(moves, square, singleTarget, piece, pieces[singleTarget], CASTLE_NONE);

            if (y == startRank)
            {
                int doubleTargetY = y + (forward * 2);
                if (IsOnBoard(x, doubleTargetY))
                {
                    Square doubleTarget = FlattenSquare(x, doubleTargetY);
                    Bitboard doubleMask = SquareMask(doubleTarget);
                    if (!(occupancy & doubleMask))
                        AddMove(moves, square, doubleTarget, piece, pieces[doubleTarget], CASTLE_NONE);
                }
            }
        }

        Bitboard captureTargets = pawn_attack_lookup[(turn == TURN_WHITE) ? 0 : 1][square] & ~friendlyOccupancy;
        while (captureTargets)
        {
            Square to = PopLeastSignificantBit(captureTargets);
            if (occupancy & SquareMask(to))
                AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }

        if (en_passant < 64 &&
            (pawn_attack_lookup[(turn == TURN_WHITE) ? 0 : 1][square] &
             SquareMask(en_passant)))
        {
            Square capturedSquare = FlattenSquare(
                get_piece_x(en_passant), get_piece_y(square));
            Piece captured = pieces[capturedSquare];
            if (get_piece_type(captured) == PIECE_TYPE_PAWN &&
                PieceIsOpponent(captured, turn))
            {
                AddMove(moves, square, en_passant, piece, captured, CASTLE_NONE);
                moves.back().flags = MOVE_EN_PASSANT;
            }
        }
    }
}


void ChessBoard::GetLegalKnightMoves(std::vector<Move>& moves)
{
    
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;
    Bitboard knights = occupancy_bitboards[
        PIECE_TYPE_KNIGHT | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (knights)
    {
        Square square = PopLeastSignificantBit(knights);
        Piece piece = pieces[square];
        Bitboard targets = knight_attack_lookup[square] & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalBishopMoves(std::vector<Move>& moves)
{
    
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;

    Bitboard bishops = occupancy_bitboards[
        PIECE_TYPE_BISHOP | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (bishops)
    {
        Square square = PopLeastSignificantBit(bishops);
        Piece piece = pieces[square];
        Bitboard targets = SlidingAttacks(square, occupancy, true) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalRookMoves(std::vector<Move>& moves)
{
    
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;

    Bitboard rooks = occupancy_bitboards[
        PIECE_TYPE_ROOK | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (rooks)
    {
        Square square = PopLeastSignificantBit(rooks);
        Piece piece = pieces[square];
        Bitboard targets = SlidingAttacks(square, occupancy, false) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalQueenMoves(std::vector<Move>& moves)
{
    
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;

    Bitboard queens = occupancy_bitboards[
        PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (queens)
    {
        Square square = PopLeastSignificantBit(queens);
        Piece piece = pieces[square];
        Bitboard targets = (SlidingAttacks(square, occupancy, true) |
            SlidingAttacks(square, occupancy, false)) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalKingMoves(std::vector<Move>& moves)
{
    Bitboard friendlyOccupancy =
        (turn == TURN_WHITE)
            ? occupancy_bitboard_white
            : occupancy_bitboard_black;

    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING |
        (turn == TURN_WHITE
            ? PIECE_COLOR_WHITE
            : PIECE_COLOR_BLACK)
    ];

    while (kings)
    {
        Square square = PopLeastSignificantBit(kings);
        Piece piece = pieces[square];

        Bitboard targets =
            king_attack_lookup[square] &
            ~friendlyOccupancy;

        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);

            AddMove(
                moves,
                square,
                to,
                piece,
                pieces[to],
                CASTLE_NONE
            );
        }

        AddCastlingMoves(moves, square, piece);
    }
}


void ChessBoard::AddCastlingMoves(
    std::vector<Move>& moves,
    Square kingSquare,
    Piece king)
{
    if (turn == TURN_WHITE && kingSquare == E1)
    {
        // White kingside: e1 -> g1
        if (castling_rights & CASTLE_WK)
        {
            if (pieces[F1] == NULL_PIECE &&
                pieces[G1] == NULL_PIECE &&
                pieces[H1] ==
                    (PIECE_COLOR_WHITE | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(E1, TURN_BLACK) &&
                !IsSquareAttacked(F1, TURN_BLACK) &&
                !IsSquareAttacked(G1, TURN_BLACK))
            {
                Move move;
                move.from = E1;
                move.to = G1;
                move.moved = king;
                move.captured = NULL_PIECE;
                move.flags = MOVE_CASTLE_KINGSIDE;
                move.prev_castling_rights = castling_rights;

                moves.push_back(move);
            }
        }

        // White queenside: e1 -> c1
        if (castling_rights & CASTLE_WQ)
        {
            if (pieces[D1] == NULL_PIECE &&
                pieces[C1] == NULL_PIECE &&
                pieces[B1] == NULL_PIECE &&
                pieces[A1] ==
                    (PIECE_COLOR_WHITE | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(E1, TURN_BLACK) &&
                !IsSquareAttacked(D1, TURN_BLACK) &&
                !IsSquareAttacked(C1, TURN_BLACK))
            {
                Move move;
                move.from = E1;
                move.to = C1;
                move.moved = king;
                move.captured = NULL_PIECE;
                move.flags = MOVE_CASTLE_QUEENSIDE;
                move.prev_castling_rights = castling_rights;

                moves.push_back(move);
            }
        }
    }

    if (turn == TURN_BLACK && kingSquare == E8)
    {
        // Black kingside: e8 -> g8
        if (castling_rights & CASTLE_BK)
        {
            if (pieces[F8] == NULL_PIECE &&
                pieces[G8] == NULL_PIECE &&
                pieces[H8] ==
                    (PIECE_COLOR_BLACK | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(E8, TURN_WHITE) &&
                !IsSquareAttacked(F8, TURN_WHITE) &&
                !IsSquareAttacked(G8, TURN_WHITE))
            {
                Move move;
                move.from = E8;
                move.to = G8;
                move.moved = king;
                move.captured = NULL_PIECE;
                move.flags = MOVE_CASTLE_KINGSIDE;
                move.prev_castling_rights = castling_rights;

                moves.push_back(move);
            }
        }

        // Black queenside: e8 -> c8
        if (castling_rights & CASTLE_BQ)
        {
            if (pieces[D8] == NULL_PIECE &&
                pieces[C8] == NULL_PIECE &&
                pieces[B8] == NULL_PIECE &&
                pieces[A8] ==
                    (PIECE_COLOR_BLACK | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(E8, TURN_WHITE) &&
                !IsSquareAttacked(D8, TURN_WHITE) &&
                !IsSquareAttacked(C8, TURN_WHITE))
            {
                Move move;
                move.from = E8;
                move.to = C8;
                move.moved = king;
                move.captured = NULL_PIECE;
                move.flags = MOVE_CASTLE_QUEENSIDE;
                move.prev_castling_rights = castling_rights;

                moves.push_back(move);
            }
        }
    }
}


std::vector<Move> ChessBoard::GetLegalMoves()
{
    std::vector<Move> moves;
    moves.reserve(GetNumMoves());

    GetLegalPawnMoves(moves);
    GetLegalKnightMoves(moves);
    GetLegalBishopMoves(moves);
    GetLegalRookMoves(moves);
    GetLegalQueenMoves(moves);
    GetLegalKingMoves(moves);

    size_t write = 0;

    for (size_t read = 0; read < moves.size(); ++read)
    {
        Move move = moves[read];

        MakeMove(move);

        // After MakeMove, `turn` is the opponent.
        TurnColor moverColor =
            (turn == TURN_WHITE) ? TURN_BLACK : TURN_WHITE;

        Bitboard moverKings = occupancy_bitboards[
            PIECE_TYPE_KING |
            (moverColor == TURN_WHITE
                ? PIECE_COLOR_WHITE
                : PIECE_COLOR_BLACK)
        ];

        bool in_check = false;

        if (moverKings)
        {
            Square kingSq =
                Square(__builtin_ctzll(moverKings));

            Bitboard opponentAttacks =
                (turn == TURN_WHITE)
                    ? attack_bitboard_white
                    : attack_bitboard_black;

            in_check =
                opponentAttacks & SquareMask(kingSq);
        }

        UndoMove(move);

        if (!in_check)
        {
            moves[write++] = move;
        }
    }

    moves.resize(write);
    return moves;
}


std::vector<Move> ChessBoard::GetLegalCaptures()
{
    std::vector<Move> moves;
    moves.reserve(GetNumCaptures());

    GetLegalPawnMoves(moves);
    GetLegalKnightMoves(moves);
    GetLegalBishopMoves(moves);
    GetLegalRookMoves(moves);
    GetLegalQueenMoves(moves);
    GetLegalKingMoves(moves);

    size_t write = 0;

    for (size_t read = 0; read < moves.size(); ++read)
    {
        Move move = moves[read];

        if (move.captured == NULL_PIECE)
            continue;

        MakeMove(move);

        TurnColor moverColor =
            (turn == TURN_WHITE) ? TURN_BLACK : TURN_WHITE;

        Bitboard moverKings = occupancy_bitboards[
            PIECE_TYPE_KING |
            (moverColor == TURN_WHITE
                ? PIECE_COLOR_WHITE
                : PIECE_COLOR_BLACK)
        ];

        bool in_check = false;

        if (moverKings)
        {
            Square kingSq =
                Square(__builtin_ctzll(moverKings));

            Bitboard opponentAttacks =
                (turn == TURN_WHITE)
                    ? attack_bitboard_white
                    : attack_bitboard_black;

            in_check =
                opponentAttacks & SquareMask(kingSq);
        }

        UndoMove(move);

        if (!in_check)
        {
            moves[write++] = move;
        }
    }

    moves.resize(write);
    return moves;
}


size_t ChessBoard::GetNumMoves() const
{
    const Bitboard white = occupancy_bitboard_white;
    const Bitboard black = occupancy_bitboard_black;

    const Bitboard own   = (turn == TURN_WHITE) ? white : black;

    const int color =
        (turn == TURN_WHITE) ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK;

    // If attack_bitboards contains the union of attacks for each piece type:
    const Bitboard pawn_attacks   = attack_bitboards[PIECE_TYPE_PAWN   | color];
    const Bitboard knight_attacks = attack_bitboards[PIECE_TYPE_KNIGHT | color];
    const Bitboard bishop_attacks = attack_bitboards[PIECE_TYPE_BISHOP | color];
    const Bitboard rook_attacks   = attack_bitboards[PIECE_TYPE_ROOK   | color];
    const Bitboard queen_attacks  = attack_bitboards[PIECE_TYPE_QUEEN  | color];
    const Bitboard king_attacks   = attack_bitboards[PIECE_TYPE_KING   | color];

    const Bitboard attacks =
        pawn_attacks |
        knight_attacks |
        bishop_attacks |
        rook_attacks |
        queen_attacks |
        king_attacks;

    // Can't move onto a square occupied by one of our own pieces.
    const Bitboard moves = attacks & ~own;

    return std::popcount(moves);
}


size_t ChessBoard::GetNumCaptures() const
{
    const Bitboard white = occupancy_bitboard_white;
    const Bitboard black = occupancy_bitboard_black;

    const Bitboard own   = (turn == TURN_WHITE) ? white : black;
    const Bitboard enemy = (turn == TURN_WHITE) ? black : white;

    const int color =
        (turn == TURN_WHITE) ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK;

    // If attack_bitboards contains the union of attacks for each piece type:
    const Bitboard pawn_attacks   = attack_bitboards[PIECE_TYPE_PAWN   | color];
    const Bitboard knight_attacks = attack_bitboards[PIECE_TYPE_KNIGHT | color];
    const Bitboard bishop_attacks = attack_bitboards[PIECE_TYPE_BISHOP | color];
    const Bitboard rook_attacks   = attack_bitboards[PIECE_TYPE_ROOK   | color];
    const Bitboard queen_attacks  = attack_bitboards[PIECE_TYPE_QUEEN  | color];
    const Bitboard king_attacks   = attack_bitboards[PIECE_TYPE_KING   | color];

    const Bitboard attacks =
        pawn_attacks |
        knight_attacks |
        bishop_attacks |
        rook_attacks |
        queen_attacks |
        king_attacks;

    // Can't move onto a square occupied by one of our own pieces.
    const Bitboard moves = attacks & ~own;

    return std::popcount(moves & enemy);
}


void ChessBoard::GetNumLegalMovesAndCaptures(
    size_t& moves_count,
    size_t& captures_count) const
{
    const Bitboard white = occupancy_bitboard_white;
    const Bitboard black = occupancy_bitboard_black;

    const Bitboard own   = (turn == TURN_WHITE) ? white : black;
    const Bitboard enemy = (turn == TURN_WHITE) ? black : white;

    const int color =
        (turn == TURN_WHITE) ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK;

    // If attack_bitboards contains the union of attacks for each piece type:
    const Bitboard pawn_attacks   = attack_bitboards[PIECE_TYPE_PAWN   | color];
    const Bitboard knight_attacks = attack_bitboards[PIECE_TYPE_KNIGHT | color];
    const Bitboard bishop_attacks = attack_bitboards[PIECE_TYPE_BISHOP | color];
    const Bitboard rook_attacks   = attack_bitboards[PIECE_TYPE_ROOK   | color];
    const Bitboard queen_attacks  = attack_bitboards[PIECE_TYPE_QUEEN  | color];
    const Bitboard king_attacks   = attack_bitboards[PIECE_TYPE_KING   | color];

    const Bitboard attacks =
        pawn_attacks |
        knight_attacks |
        bishop_attacks |
        rook_attacks |
        queen_attacks |
        king_attacks;

    // Can't move onto a square occupied by one of our own pieces.
    const Bitboard moves = attacks & ~own;

    moves_count = std::popcount(moves);
    captures_count = std::popcount(moves & enemy);
}


bool ChessBoard::HasPseudoLegalCapture() const
{
    //const Bitboard own =
    //    (turn == TURN_WHITE)
    //        ? occupancy_bitboard_white
    //        : occupancy_bitboard_black;

    const Bitboard enemy =
        (turn == TURN_WHITE)
            ? occupancy_bitboard_black
            : occupancy_bitboard_white;

    const int color =
        (turn == TURN_WHITE)
            ? PIECE_COLOR_WHITE
            : PIECE_COLOR_BLACK;

    const Bitboard attacks =
        attack_bitboards[PIECE_TYPE_PAWN | color]   |
        attack_bitboards[PIECE_TYPE_KNIGHT | color] |
        attack_bitboards[PIECE_TYPE_BISHOP | color] |
        attack_bitboards[PIECE_TYPE_ROOK | color]   |
        attack_bitboards[PIECE_TYPE_QUEEN | color]  |
        attack_bitboards[PIECE_TYPE_KING | color];

    return (attacks & enemy) != 0;
}


bool ChessBoard::IsLegalMove(Move move)
{
    std::vector<Move> moves = GetLegalMoves();

    for (Move move_ : moves)
    {
        if ((move.from == move_.from) && (move.to == move_.to))
            return true;
    }

    return false;
}


bool ChessBoard::IsCheck()
{
    UpdateAttackBitboards();

    Bitboard opponentAttacks = (turn == TURN_WHITE) ? attack_bitboard_black : attack_bitboard_white;

    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    if (!kings)
        return false;

    Square kingSq = Square(__builtin_ctzll(kings));
    return (opponentAttacks & SquareMask(kingSq)) != 0;
}


bool ChessBoard::IsCheckMate()
{
    if (!IsCheck())
        return false;

    std::vector<Move> legal = GetLegalMoves();
    return legal.empty();
}


bool ChessBoard::IsStaleMate()
{
    if (IsCheck())
        return false;

    std::vector<Move> legal = GetLegalMoves();
    return legal.empty();
}


bool ChessBoard::IsThreeFoldRepition() const
{
    std::unordered_map<ZobristHash, size_t> positions_map;

    for (const ZobristHash hash : history)
    {
        if (++positions_map[hash] >= 3)
            return true;
    }

    return false;
}


uint8_t ChessBoard::CountPawns() const
{
    Bitboard pawns = occupancy_bitboards[
            PIECE_TYPE_PAWN |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    uint8_t count = 0;
    while (pawns)
    {
        PopLeastSignificantBit(pawns);
        ++count;
    }
    return count;
}


uint8_t ChessBoard::CountKnights() const
{
    Bitboard knights = occupancy_bitboards[
            PIECE_TYPE_KNIGHT |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    uint8_t count = 0;
    while (knights)
    {
        PopLeastSignificantBit(knights);
        ++count;
    }
    return count;
}


uint8_t ChessBoard::CountBishops() const
{
    Bitboard bishops = occupancy_bitboards[
            PIECE_TYPE_BISHOP |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    uint8_t count = 0;
    while (bishops)
    {
        PopLeastSignificantBit(bishops);
        ++count;
    }
    return count;
}


uint8_t ChessBoard::CountRooks() const
{
    Bitboard rooks = occupancy_bitboards[
            PIECE_TYPE_ROOK |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    uint8_t count = 0;
    while (rooks)
    {
        PopLeastSignificantBit(rooks);
        ++count;
    }
    return count;
}


uint8_t ChessBoard::CountQueens() const
{
    Bitboard queens = occupancy_bitboards[
            PIECE_TYPE_QUEEN |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    uint8_t count = 0;
    while (queens)
    {
        PopLeastSignificantBit(queens);
        ++count;
    }
    return count;
}


uint8_t ChessBoard::CountKings() const
{
    Bitboard kings = occupancy_bitboards[
            PIECE_TYPE_KING |
            (turn == TURN_WHITE ?
                PIECE_COLOR_WHITE :
                PIECE_COLOR_BLACK)
        ];
    uint8_t count = 0;
    while (kings)
    {
        PopLeastSignificantBit(kings);
        ++count;
    }
    return count;
}


Square ChessBoard::PopPawns() const
{
    Bitboard pawns = occupancy_bitboards[
        PIECE_TYPE_PAWN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(pawns);
}


Square ChessBoard::PopKnights() const
{
    Bitboard knights = occupancy_bitboards[
        PIECE_TYPE_KNIGHT | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(knights);
}


Square ChessBoard::PopBishops() const
{
    Bitboard bishops = occupancy_bitboards[
        PIECE_TYPE_BISHOP | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(bishops);
}


Square ChessBoard::PopRooks() const
{
    Bitboard rooks = occupancy_bitboards[
        PIECE_TYPE_ROOK | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(rooks);
}


Square ChessBoard::PopQueens() const
{
    Bitboard queens = occupancy_bitboards[
        PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(queens);
}


Square ChessBoard::PopKings() const
{
    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(kings);
}


Bitboard ChessBoard::GetPawns() const
{
    return occupancy_bitboards[
        PIECE_TYPE_PAWN |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];
}


Bitboard ChessBoard::GetKnights() const
{
    return occupancy_bitboards[
        PIECE_TYPE_KNIGHT |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];
}


Bitboard ChessBoard::GetBishops() const
{
    return occupancy_bitboards[
        PIECE_TYPE_BISHOP |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];
}


Bitboard ChessBoard::GetRooks() const
{
    return occupancy_bitboards[
        PIECE_TYPE_ROOK |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];
}


Bitboard ChessBoard::GetQueens() const
{
    return occupancy_bitboards[
        PIECE_TYPE_QUEEN |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];
}


Bitboard ChessBoard::GetKings() const
{
    return occupancy_bitboards[
        PIECE_TYPE_KING |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];
}

