#include "chess.hpp"

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
static constexpr Bitboard bishop_magics[64] = {
    0x0041100400802040ULL, 0x0008c12104010007ULL,
    0x1304210202100080ULL, 0x0004104204101201ULL,
    0x90060a1000000202ULL, 0x5402080444c24000ULL,
    0x0009080804840000ULL, 0x1822004c22011000ULL,
    0x2401e04902081040ULL, 0x0a10202e04051020ULL,
    0x0020100082084908ULL, 0x0508110410800000ULL,
    0x000001104000a000ULL, 0x8113508220201206ULL,
    0x0042108088203288ULL, 0x0002208284109228ULL,
    0x18500c0410500141ULL, 0x0002001110210100ULL,
    0x02010810084a0040ULL, 0x000400012c008010ULL,
    0x00140002050c0242ULL, 0x060241020100a000ULL,
    0x0000a40108081a41ULL, 0x9082007022020280ULL,
    0x0420042308104488ULL, 0x0204120010100565ULL,
    0x8004300108108020ULL, 0x0064080004010410ULL,
    0x0280802022020040ULL, 0x200c00a025101000ULL,
    0x4001490a2a080100ULL, 0x100a160009405a00ULL,
    0x02010868406120c0ULL, 0x0001011005081048ULL,
    0x04a1080100021402ULL, 0x1022008020020200ULL,
    0x400c008400060030ULL, 0x0502020408020080ULL,
    0x08024214080200a0ULL, 0x200080830000840eULL,
    0x0006100d04882061ULL, 0x0528840402082094ULL,
    0x00000a008214d000ULL, 0x100011a011080808ULL,
    0x0008200208810401ULL, 0x2004992442004500ULL,
    0x0205080801000050ULL, 0x001a120442080110ULL,
    0x1004020882084280ULL, 0x20b0210442200804ULL,
    0x9000010841100100ULL, 0x8002401020880000ULL,
    0x000c0e1220220020ULL, 0x2808120210430300ULL,
    0x0089420428020092ULL, 0x0802084801214408ULL,
    0x0001008801011001ULL, 0x4002060a12220222ULL,
    0x8890081024022200ULL, 0x022101000020a810ULL,
    0x0004880842104100ULL, 0x100001100410042cULL,
    0x0008c29001210100ULL, 0x0822100111050208ULL
};
static constexpr Bitboard rook_magics[64] = {
    0x00800080221a4000ULL, 0x2040002000401000ULL,
    0xa900090010422000ULL, 0x0200041140200a00ULL,
    0x1001008040200810ULL, 0x0200100200080401ULL,
    0x0280108002000100ULL, 0x2080088000402700ULL,
    0x200a002080420100ULL, 0x400c808040002000ULL,
    0x0216801001200080ULL, 0x8201001000082100ULL,
    0x2c40800800800400ULL, 0x0060808002000400ULL,
    0x9021800100800200ULL, 0x6601000040810002ULL,
    0x2080004020004001ULL, 0x1010024000402009ULL,
    0x2000808020001000ULL, 0x0026020040201208ULL,
    0x5004008004080081ULL, 0x0100808004000200ULL,
    0x0000040002100801ULL, 0x4800020010440389ULL,
    0x6200400080008020ULL, 0x00a0008280400220ULL,
    0x0040120200208040ULL, 0x0000080280100080ULL,
    0x0010040180080180ULL, 0x1020020080040080ULL,
    0x1002880c00101902ULL, 0x81020c8600006104ULL,
    0x0000400088800728ULL, 0x1a10002001400050ULL,
    0x0080100080802000ULL, 0x0020811001800800ULL,
    0x0000040080800800ULL, 0x0000040080800200ULL,
    0x000a820001010004ULL, 0x801004005a000081ULL,
    0x0080400020908000ULL, 0x6840201000404004ULL,
    0x0100208042020018ULL, 0x8088020100101000ULL,
    0x0004000800048080ULL, 0x0001000400090002ULL,
    0x0183100802440001ULL, 0x04820400a0420011ULL,
    0x1311042842008200ULL, 0x1004402213810200ULL,
    0x0500100080200080ULL, 0x0e08000810008080ULL,
    0x0100080004008080ULL, 0x0482001004080200ULL,
    0x1010214210280400ULL, 0x2000008401004200ULL,
    0x4424805301420022ULL, 0x1040102100804003ULL,
    0x0800084011002001ULL, 0x0012012008100442ULL,
    0x0032002008041002ULL, 0x3206000130082422ULL,
    0x000c101801122084ULL, 0x0008084030840102ULL
};
static uint8_t bishop_magic_shifts[64];
static uint8_t rook_magic_shifts[64];
static Bitboard bishop_attacks[64][8192];
static Bitboard rook_attacks[64][16384];
static bool attack_lookup_initialized = false;

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
    (1ULL << SQ_F1) | (1ULL << SQ_G1);

constexpr Bitboard WHITE_QUEENSIDE_EMPTY =
    (1ULL << SQ_B1) | (1ULL << SQ_C1) | (1ULL << SQ_D1);

constexpr Bitboard BLACK_KINGSIDE_EMPTY  =
    (1ULL << SQ_F8) | (1ULL << SQ_G8);

constexpr Bitboard BLACK_QUEENSIDE_EMPTY =
    (1ULL << SQ_B8) | (1ULL << SQ_C8) | (1ULL << SQ_D8);

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
        for (int square = 0; square < 64; ++square)
            attack_count_by_piece[i][square] = 0;
    }

    occupancy_bitboard_white = 0ULL;
    occupancy_bitboard_black = 0ULL;
    attack_bitboard_white = 0ULL;
    attack_bitboard_black = 0ULL;
    for (int color = 0; color < 2; ++color)
        for (int square = 0; square < 64; ++square)
            attack_count_by_color[color][square] = 0;
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
    fullmove_number = 0;
    halfmove_clock = 0;

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
    UpdateOccupancyBitboards();
    UpdateAttackBitboards();

    zobrist_hash = GenerateZobristHash();
    history.push_back(zobrist_hash);

    return true;
}


std::string ChessBoard::GetFEN() const
{
    std::ostringstream fen;

    // Piece placement
    for (int rank = 7; rank >= 0; --rank)
    {
        int empty = 0;

        for (int file = 0; file < 8; ++file)
        {
            const Square square = Square((rank << 3) | file);
            const Piece piece = pieces[square];

            if (piece == NULL_PIECE)
            {
                ++empty;
                continue;
            }

            if (empty != 0)
            {
                fen << empty;
                empty = 0;
            }

            char piece_char;

            switch (get_piece_type(piece))
            {
                case PIECE_TYPE_PAWN:
                    piece_char = 'p';
                    break;

                case PIECE_TYPE_KNIGHT:
                    piece_char = 'n';
                    break;

                case PIECE_TYPE_BISHOP:
                    piece_char = 'b';
                    break;

                case PIECE_TYPE_ROOK:
                    piece_char = 'r';
                    break;

                case PIECE_TYPE_QUEEN:
                    piece_char = 'q';
                    break;

                case PIECE_TYPE_KING:
                    piece_char = 'k';
                    break;

                default:
                    piece_char = '?';
                    break;
            }

            if (get_piece_color(piece) == PIECE_COLOR_WHITE)
                piece_char = static_cast<char>(std::toupper(piece_char));

            fen << piece_char;
        }

        if (empty != 0)
            fen << empty;

        if (rank != 0)
            fen << '/';
    }

    // Side to move
    fen << ' '
        << (turn == TURN_WHITE ? 'w' : 'b');

    // Castling rights
    fen << ' ';

    if (castling_rights == CASTLE_NONE)
    {
        fen << '-';
    }
    else
    {
        if (castling_rights & CASTLE_WK)
            fen << 'K';

        if (castling_rights & CASTLE_WQ)
            fen << 'Q';

        if (castling_rights & CASTLE_BK)
            fen << 'k';

        if (castling_rights & CASTLE_BQ)
            fen << 'q';
    }

    // En-passant target
    fen << ' ';

    if (en_passant == 64)
    {
        fen << '-';
    }
    else
    {
        const int file = get_piece_x(en_passant);
        const int rank = get_piece_y(en_passant);

        fen << static_cast<char>('a' + file)
            << static_cast<char>('1' + rank);
    }

    // Halfmove clock
    fen << ' ' << halfmove_clock;

    // Fullmove number
    fen << ' ' << fullmove_number;

    return fen.str();
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
    attack_bitboard_white = 0ULL;
    attack_bitboard_black = 0ULL;

    for (int i = 0; i < 32; ++i)
    {
        attack_bitboards[i] = 0ULL;
        for (int square = 0; square < 64; ++square)
            attack_count_by_piece[i][square] = 0;
    }

    for (int color = 0; color < 2; ++color)
        for (int square = 0; square < 64; ++square)
            attack_count_by_color[color][square] = 0;

    for (Square square = 0; square < 64; ++square)
    {
        const Piece piece = pieces[square];
        if (piece != NULL_PIECE)
            ChangeAttackContributions(SquareMask(square), true);
    }
}


Bitboard ChessBoard::GetPieceAttacks(
    const Piece piece,
    const Square square) const
{
    const Bitboard occupancy =
        occupancy_bitboard_white | occupancy_bitboard_black;

    switch (get_piece_type(piece))
    {
        case PIECE_TYPE_PAWN:
            return pawn_attack_lookup[
                (piece & PIECE_COLOR_WHITE) == 0][square];

        case PIECE_TYPE_KNIGHT:
            return knight_attack_lookup[square];

        case PIECE_TYPE_BISHOP:
            return SlidingAttacks(square, occupancy, true);

        case PIECE_TYPE_ROOK:
            return SlidingAttacks(square, occupancy, false);

        case PIECE_TYPE_QUEEN:
            return SlidingAttacks(square, occupancy, true) |
                SlidingAttacks(square, occupancy, false);

        case PIECE_TYPE_KING:
            return king_attack_lookup[square];

        default:
            return 0ULL;
    }
}


Bitboard ChessBoard::GetAffectedAttackSquares(
    const Square* changed_squares,
    const size_t changed_count) const
{
    static constexpr int directions[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    Bitboard affected = 0ULL;
    for (size_t index = 0; index < changed_count; ++index)
    {
        const Square changed = changed_squares[index];
        affected |= SquareMask(changed);

        const int x = get_piece_x(changed);
        const int y = get_piece_y(changed);
        for (const auto& direction : directions)
        {
            int targetX = x + direction[0];
            int targetY = y + direction[1];
            while (IsOnBoard(targetX, targetY))
            {
                const Square target = FlattenSquare(targetX, targetY);
                const Piece piece = pieces[target];
                const bool diagonal = direction[0] != 0 && direction[1] != 0;
                const Piece type = get_piece_type(piece);
                if (type == PIECE_TYPE_QUEEN ||
                    (diagonal && type == PIECE_TYPE_BISHOP) ||
                    (!diagonal && type == PIECE_TYPE_ROOK))
                {
                    affected |= SquareMask(target);
                }

                targetX += direction[0];
                targetY += direction[1];
            }
        }
    }

    return affected;
}


void ChessBoard::ChangeAttackContributions(
    const Bitboard squares,
    const bool add)
{
    Bitboard remaining = squares;
    while (remaining)
    {
        const Square square = PopLSB(remaining);
        const Piece piece = pieces[square];
        if (piece == NULL_PIECE)
            continue;

        const int color = (piece & PIECE_COLOR_WHITE) ? 0 : 1;
        Bitboard attacks = GetPieceAttacks(piece, square);
        while (attacks)
        {
            const Square target = PopLSB(attacks);
            uint8_t& pieceCount = attack_count_by_piece[piece][target];
            uint8_t& colorCount = attack_count_by_color[color][target];

            if (add)
            {
                if (pieceCount++ == 0)
                    attack_bitboards[piece] |= SquareMask(target);
                if (colorCount++ == 0)
                {
                    if (color == 0)
                        attack_bitboard_white |= SquareMask(target);
                    else
                        attack_bitboard_black |= SquareMask(target);
                }
            }
            else
            {
                if (--pieceCount == 0)
                    attack_bitboards[piece] &= ~SquareMask(target);
                if (--colorCount == 0)
                {
                    if (color == 0)
                        attack_bitboard_white &= ~SquareMask(target);
                    else
                        attack_bitboard_black &= ~SquareMask(target);
                }
            }
        }
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

        const unsigned bishopBits = GetNumSetBits(bishop_masks[square]);
        const unsigned rookBits = GetNumSetBits(rook_masks[square]);
        bishop_magic_shifts[square] = 64 - bishopBits;
        rook_magic_shifts[square] = 64 - rookBits;

        const unsigned bishopSize = 1U << GetNumSetBits(bishop_masks[square]);
        const unsigned rookSize = 1U << GetNumSetBits(rook_masks[square]);
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
        if (move.from == SQ_H1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WK);

        else if (move.from == SQ_A1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WQ);

        else if (move.from == SQ_H8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BK);

        else if (move.from == SQ_A8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BQ);
    }

    // A rook being captured also removes its castling right.
    if ((capturedPiece & 0x07) == PIECE_TYPE_ROOK)
    {
        if (move.to == SQ_H1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WK);

        else if (move.to == SQ_A1)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_WQ);

        else if (move.to == SQ_H8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BK);

        else if (move.to == SQ_A8)
            castling_rights =
                CastlingRights(castling_rights & ~CASTLE_BQ);
    }

    // ------------------------------------------------------------
    // Flag updates
    // ------------------------------------------------------------

    uint8_t flags = MOVE_NORMAL;

    if ((originalPiece & 0x07) == PIECE_TYPE_KING)
    {
        if (movingWhite)
        {
            if (move.from == SQ_E1 && move.to == SQ_G1)
                flags = MOVE_CASTLE_KINGSIDE;
            else if (move.from == SQ_E1 && move.to == SQ_C1)
                flags = MOVE_CASTLE_QUEENSIDE;
        }
        else
        {
            if (move.from == SQ_E8 && move.to == SQ_G8)
                flags = MOVE_CASTLE_KINGSIDE;
            else if (move.from == SQ_E8 && move.to == SQ_C8)
                flags = MOVE_CASTLE_QUEENSIDE;
        }
    }

    if ((originalPiece & 0x07) == PIECE_TYPE_PAWN)
    {
        int fromY = get_piece_y(move.from);
        int toY = get_piece_y(move.to);

        if (move.to == en_passant && move.from != move.to &&
            pieces[move.to] == NULL_PIECE)
        {
            flags = MOVE_EN_PASSANT;
            Square capturedSquare = FlattenSquare(
                get_piece_x(move.to), get_piece_y(move.from));
            capturedPiece = pieces[capturedSquare];
            move.captured = capturedPiece;
        }

        if ((movingWhite && fromY == 6 && toY == 7) ||
            (!movingWhite && fromY == 1 && toY == 0))
        {
            flags |= MOVE_PROMOTION;

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

    move.flags = flags;

    Square changedSquares[6] = { move.from, move.to };
    size_t changedCount = 2;
    if (move.flags & MOVE_EN_PASSANT)
    {
        changedSquares[changedCount++] = FlattenSquare(
            get_piece_x(move.to), get_piece_y(move.from));
    }
    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        changedSquares[changedCount++] = movingWhite ? SQ_H1 : SQ_H8;
        changedSquares[changedCount++] = movingWhite ? SQ_F1 : SQ_F8;
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        changedSquares[changedCount++] = movingWhite ? SQ_A1 : SQ_A8;
        changedSquares[changedCount++] = movingWhite ? SQ_D1 : SQ_D8;
    }

    const Bitboard affectedAttackSquares =
        GetAffectedAttackSquares(changedSquares, changedCount);
    ChangeAttackContributions(affectedAttackSquares, false);

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
        Square rookFrom = movingWhite ? SQ_H1 : SQ_H8;
        Square rookTo   = movingWhite ? SQ_F1 : SQ_F8;

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
        Square rookFrom = movingWhite ? SQ_A1 : SQ_A8;
        Square rookTo   = movingWhite ? SQ_D1 : SQ_D8;

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

    // Toggle side to move.
    zobrist_hash ^= zobrist_keys.side_to_move;

    // Update castling rights.
    if (move.prev_castling_rights != castling_rights)
    {
        zobrist_hash ^=
            zobrist_keys.castling[move.prev_castling_rights];

        zobrist_hash ^=
            zobrist_keys.castling[castling_rights];
    }

    // Update en-passant file.
    if (move.prev_en_passant != 64)
    {
        zobrist_hash ^=
            zobrist_keys.en_passant[
                get_piece_x(move.prev_en_passant)
            ];
    }

    if (en_passant != 64)
    {
        zobrist_hash ^=
            zobrist_keys.en_passant[
                get_piece_x(en_passant)
            ];
    }

    // Remove moving piece from source.
    zobrist_hash ^=
        zobrist_keys.pieces[
            GetZobristPieceIndex(originalPiece)
        ][move.from];

    // Add final piece to destination.
    zobrist_hash ^=
        zobrist_keys.pieces[
            GetZobristPieceIndex(movedPiece)
        ][move.to];

    // Remove captured piece from its actual square.
    if (capturedPiece != NULL_PIECE)
    {
        Square capturedSquare = move.to;

        if (move.flags & MOVE_EN_PASSANT)
        {
            capturedSquare = FlattenSquare(
                get_piece_x(move.to),
                get_piece_y(move.from));
        }

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(capturedPiece)
            ][capturedSquare];
    }

    // Castling rook.
    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        Square rookFrom = movingWhite ? SQ_H1 : SQ_H8;
        Square rookTo   = movingWhite ? SQ_F1 : SQ_F8;

        Piece rook = pieces[rookTo];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookFrom];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookTo];
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        Square rookFrom = movingWhite ? SQ_A1 : SQ_A8;
        Square rookTo   = movingWhite ? SQ_D1 : SQ_D8;

        Piece rook = pieces[rookTo];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookFrom];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookTo];
    }

    move.moved = movedPiece;
    move.captured = capturedPiece;

    turn ^= 1;

    fullmove_number += turn == TURN_BLACK;

    ChangeAttackContributions(affectedAttackSquares, true);
    history.push_back(zobrist_hash);
}


void ChessBoard::UndoMove(Move move)
{
    bool movingWhite = (turn == TURN_BLACK);

    Piece movedPiece = move.moved;

    Square changedSquares[6] = { move.from, move.to };
    size_t changedCount = 2;
    if (move.flags & MOVE_EN_PASSANT)
    {
        changedSquares[changedCount++] = FlattenSquare(
            get_piece_x(move.to), get_piece_y(move.from));
    }
    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        changedSquares[changedCount++] = movingWhite ? SQ_H1 : SQ_H8;
        changedSquares[changedCount++] = movingWhite ? SQ_F1 : SQ_F8;
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        changedSquares[changedCount++] = movingWhite ? SQ_A1 : SQ_A8;
        changedSquares[changedCount++] = movingWhite ? SQ_D1 : SQ_D8;
    }

    const Bitboard affectedAttackSquares =
        GetAffectedAttackSquares(changedSquares, changedCount);
    ChangeAttackContributions(affectedAttackSquares, false);

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
            rookFrom = SQ_H1;
            rookTo = SQ_F1;
        }
        else
        {
            rookFrom = SQ_H8;
            rookTo = SQ_F8;
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
            rookFrom = SQ_A1;
            rookTo = SQ_D1;
        }
        else
        {
            rookFrom = SQ_A8;
            rookTo = SQ_D8;
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

    // Toggle side to move.
    zobrist_hash ^= zobrist_keys.side_to_move;

    // Remove current castling rights.
    zobrist_hash ^=
        zobrist_keys.castling[castling_rights];

    // Restore previous castling rights.
    zobrist_hash ^=
        zobrist_keys.castling[move.prev_castling_rights];

    // Remove current en-passant file.
    if (en_passant != 64)
    {
        zobrist_hash ^=
            zobrist_keys.en_passant[
                get_piece_x(en_passant)
            ];
    }

    // Restore previous en-passant file.
    if (move.prev_en_passant != 64)
    {
        zobrist_hash ^=
            zobrist_keys.en_passant[
                get_piece_x(move.prev_en_passant)
            ];
    }

    // Remove moved piece from destination.
    zobrist_hash ^=
        zobrist_keys.pieces[
            GetZobristPieceIndex(move.moved)
        ][move.to];

    // Determine original piece.
    Piece originalPiece = move.moved;

    if (move.flags & MOVE_PROMOTION)
    {
        originalPiece =
            PIECE_TYPE_PAWN |
            (movingWhite
                ? PIECE_COLOR_WHITE
                : PIECE_COLOR_BLACK);
    }

    // Restore original piece at source.
    zobrist_hash ^=
        zobrist_keys.pieces[
            GetZobristPieceIndex(originalPiece)
        ][move.from];

    // Restore captured piece.
    if (move.captured != NULL_PIECE)
    {
        Square capturedSquare = move.to;

        if (move.flags & MOVE_EN_PASSANT)
        {
            capturedSquare = FlattenSquare(
                get_piece_x(move.to),
                get_piece_y(move.from));
        }

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(move.captured)
            ][capturedSquare];
    }

    // Undo castling rook.
    if (move.flags & MOVE_CASTLE_KINGSIDE)
    {
        Square rookFrom = movingWhite ? SQ_H1 : SQ_H8;
        Square rookTo   = movingWhite ? SQ_F1 : SQ_F8;

        Piece rook = pieces[rookFrom];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookFrom];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookTo];
    }
    else if (move.flags & MOVE_CASTLE_QUEENSIDE)
    {
        Square rookFrom = movingWhite ? SQ_A1 : SQ_A8;
        Square rookTo   = movingWhite ? SQ_D1 : SQ_D8;

        Piece rook = pieces[rookFrom];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookFrom];

        zobrist_hash ^=
            zobrist_keys.pieces[
                GetZobristPieceIndex(rook)
            ][rookTo];
    }

    // Restore castling rights exactly as they were.
    castling_rights = move.prev_castling_rights;
    en_passant = move.prev_en_passant;

    turn ^= 1;

    fullmove_number -= turn == TURN_WHITE;

    ChangeAttackContributions(affectedAttackSquares, true);

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
        square = PopLSB(pawns);

        Piece piece = pieces[square];
        if ((piece & 0x07) != PIECE_TYPE_PAWN)
            continue;
        if (!PieceIsFriendly(piece, turn))
            continue;

        Bitboard targets = pawn_attack_lookup[pawnIndex][square] & opponentOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }

    if (en_passant < 64)
    {
        Bitboard enPassantPawns = occupancy_bitboards[
            PIECE_TYPE_PAWN |
            (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
        ];
        while (enPassantPawns)
        {
            const Square square = PopLSB(enPassantPawns);
            if (!(pawn_attack_lookup[pawnIndex][square] &
                  SquareMask(en_passant)))
            {
                continue;
            }

            const Square capturedSquare = FlattenSquare(
                get_piece_x(en_passant), get_piece_y(square));
            const Piece captured = pieces[capturedSquare];
            if (get_piece_type(captured) != PIECE_TYPE_PAWN ||
                !PieceIsOpponent(captured, turn))
            {
                continue;
            }

            AddMove(
                moves,
                square,
                en_passant,
                pieces[square],
                captured,
                CASTLE_NONE);
            moves.back().flags = MOVE_EN_PASSANT;
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
        Square square = PopLSB(knights);
        Piece piece = pieces[square];
        Bitboard targets = knight_attack_lookup[square] & opponentOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
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
        Square square = PopLSB(bishops);
        Piece piece = pieces[square];
        Bitboard targets = SlidingAttacks(square, occupancy, true) & opponentOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


void ChessBoard::GetLegalRookAttacks(std::vector<Move>& moves)
{
    const Bitboard opponentOccupancy =
        (turn == TURN_WHITE)
            ? occupancy_bitboard_black
            : occupancy_bitboard_white;
    const Bitboard occupancy =
        occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard rooks = occupancy_bitboards[
        PIECE_TYPE_ROOK |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (rooks)
    {
        const Square square = PopLSB(rooks);
        const Piece piece = pieces[square];
        Bitboard targets =
            SlidingAttacks(square, occupancy, false) & opponentOccupancy;
        while (targets)
        {
            const Square to = PopLSB(targets);
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
        Square square = PopLSB(queens);
        Piece piece = pieces[square];
        Bitboard targets = (SlidingAttacks(square, occupancy, true) |
            SlidingAttacks(square, occupancy, false)) & opponentOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
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
        Square square = PopLSB(kings);
        Piece piece = pieces[square];
        Bitboard targets = king_attack_lookup[square] & opponentOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
            AddMove(moves, square, to, piece, pieces[to], CASTLE_NONE);
        }
    }
}


bool ChessBoard::IsSquareAttacked(Square square, TurnColor byColor)
{
    if (square >= 64)
        return false;

    const Bitboard attacks =
        byColor == TURN_WHITE
            ? attack_bitboard_white
            : attack_bitboard_black;
    return (attacks & SquareMask(square)) != 0;
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
        Square square = PopLSB(pawns);
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
            Square to = PopLSB(captureTargets);
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
        Square square = PopLSB(knights);
        Piece piece = pieces[square];
        Bitboard targets = knight_attack_lookup[square] & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
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
        Square square = PopLSB(bishops);
        Piece piece = pieces[square];
        Bitboard targets = SlidingAttacks(square, occupancy, true) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
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
        Square square = PopLSB(rooks);
        Piece piece = pieces[square];
        Bitboard targets = SlidingAttacks(square, occupancy, false) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
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
        Square square = PopLSB(queens);
        Piece piece = pieces[square];
        Bitboard targets = (SlidingAttacks(square, occupancy, true) |
            SlidingAttacks(square, occupancy, false)) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLSB(targets);
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
        Square square = PopLSB(kings);
        Piece piece = pieces[square];

        Bitboard targets =
            king_attack_lookup[square] &
            ~friendlyOccupancy;

        while (targets)
        {
            Square to = PopLSB(targets);

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
    if (turn == TURN_WHITE && kingSquare == SQ_E1)
    {
        // White kingside: e1 -> g1
        if (castling_rights & CASTLE_WK)
        {
            if (pieces[SQ_F1] == NULL_PIECE &&
                pieces[SQ_G1] == NULL_PIECE &&
                pieces[SQ_H1] ==
                    (PIECE_COLOR_WHITE | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(SQ_E1, TURN_BLACK) &&
                !IsSquareAttacked(SQ_F1, TURN_BLACK) &&
                !IsSquareAttacked(SQ_G1, TURN_BLACK))
            {
                Move move;
                move.from = SQ_E1;
                move.to = SQ_G1;
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
            if (pieces[SQ_D1] == NULL_PIECE &&
                pieces[SQ_C1] == NULL_PIECE &&
                pieces[SQ_B1] == NULL_PIECE &&
                pieces[SQ_A1] ==
                    (PIECE_COLOR_WHITE | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(SQ_E1, TURN_BLACK) &&
                !IsSquareAttacked(SQ_D1, TURN_BLACK) &&
                !IsSquareAttacked(SQ_C1, TURN_BLACK))
            {
                Move move;
                move.from = SQ_E1;
                move.to = SQ_C1;
                move.moved = king;
                move.captured = NULL_PIECE;
                move.flags = MOVE_CASTLE_QUEENSIDE;
                move.prev_castling_rights = castling_rights;

                moves.push_back(move);
            }
        }
    }

    if (turn == TURN_BLACK && kingSquare == SQ_E8)
    {
        // Black kingside: e8 -> g8
        if (castling_rights & CASTLE_BK)
        {
            if (pieces[SQ_F8] == NULL_PIECE &&
                pieces[SQ_G8] == NULL_PIECE &&
                pieces[SQ_H8] ==
                    (PIECE_COLOR_BLACK | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(SQ_E8, TURN_WHITE) &&
                !IsSquareAttacked(SQ_F8, TURN_WHITE) &&
                !IsSquareAttacked(SQ_G8, TURN_WHITE))
            {
                Move move;
                move.from = SQ_E8;
                move.to = SQ_G8;
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
            if (pieces[SQ_D8] == NULL_PIECE &&
                pieces[SQ_C8] == NULL_PIECE &&
                pieces[SQ_B8] == NULL_PIECE &&
                pieces[SQ_A8] ==
                    (PIECE_COLOR_BLACK | PIECE_TYPE_ROOK) &&
                !IsSquareAttacked(SQ_E8, TURN_WHITE) &&
                !IsSquareAttacked(SQ_D8, TURN_WHITE) &&
                !IsSquareAttacked(SQ_C8, TURN_WHITE))
            {
                Move move;
                move.from = SQ_E8;
                move.to = SQ_C8;
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
    GetLegalMoves(moves);
    return moves;
}


void ChessBoard::GetLegalMoves(std::vector<Move>& moves)
{
    moves.clear();
    moves.reserve(GetNumMoves());

    GetLegalPawnMoves(moves);
    GetLegalKnightMoves(moves);
    GetLegalBishopMoves(moves);
    GetLegalRookMoves(moves);
    GetLegalQueenMoves(moves);
    GetLegalKingMoves(moves);

    FilterLegalMoves(moves, false);
}


std::vector<Move> ChessBoard::GetLegalCaptures()
{
    std::vector<Move> moves;
    GetLegalCaptures(moves);
    return moves;
}


void ChessBoard::GetLegalCaptures(std::vector<Move>& moves)
{
    moves.clear();
    if (!HasPseudoLegalCapture())
        return;

    moves.reserve(GetNumCaptures());

    GetLegalPawnAttacks(moves);
    GetLegalKnightAttacks(moves);
    GetLegalBishopAttacks(moves);
    GetLegalRookAttacks(moves);
    GetLegalQueenAttacks(moves);
    GetLegalKingAttacks(moves);

    FilterLegalMoves(moves, true);
}


void ChessBoard::FilterLegalMoves(
    std::vector<Move>& moves,
    const bool captures_only)
{
    size_t write = 0;

    for (size_t read = 0; read < moves.size(); ++read)
    {
        const Move originalMove = moves[read];
        Move move = originalMove;

        if (captures_only && !move.IsCapture())
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
                GetLSB(moverKings);

            Bitboard opponentAttacks =
                (turn == TURN_WHITE)
                    ? attack_bitboard_white
                    : attack_bitboard_black;

            in_check =
                opponentAttacks & SquareMask(kingSq);
        }

        UndoMove(move);

        if (!in_check)
            moves[write++] = originalMove;
    }

    moves.resize(write);
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

    return GetNumSetBits(moves);
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

    return GetNumSetBits(moves & enemy);
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

    moves_count = GetNumSetBits(moves);
    captures_count = GetNumSetBits(moves & enemy);
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

    if (attacks & enemy)
        return true;

    if (en_passant >= 64)
        return false;

    const int pawnIndex = turn == TURN_WHITE ? 0 : 1;
    Bitboard pawns = occupancy_bitboards[
        PIECE_TYPE_PAWN | color
    ];
    while (pawns)
    {
        const Square pawnSquare = PopLSB(pawns);
        if (!(pawn_attack_lookup[pawnIndex][pawnSquare] &
              SquareMask(en_passant)))
        {
            continue;
        }

        const Square capturedSquare = FlattenSquare(
            get_piece_x(en_passant), get_piece_y(pawnSquare));
        const Piece captured = pieces[capturedSquare];
        if (get_piece_type(captured) == PIECE_TYPE_PAWN &&
            PieceIsOpponent(captured, turn))
        {
            return true;
        }
    }

    return false;
}


bool ChessBoard::IsLegalMove(Move move)
{
    std::vector<Move> moves = GetLegalMoves();

    for (const Move& candidate : moves)
    {
        if (move.from == candidate.from && move.to == candidate.to)
            return true;
    }

    return false;
}


bool ChessBoard::IsCheck()
{
    Bitboard opponentAttacks = (turn == TURN_WHITE) ? attack_bitboard_black : attack_bitboard_white;

    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    if (!kings)
        return false;

    Square kingSq = GetLSB(kings);
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
    return GetNumSetBits(occupancy_bitboards[
        PIECE_TYPE_PAWN |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ]);
}


uint8_t ChessBoard::CountKnights() const
{
    return GetNumSetBits(occupancy_bitboards[
        PIECE_TYPE_KNIGHT |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ]);
}


uint8_t ChessBoard::CountBishops() const
{
    return GetNumSetBits(occupancy_bitboards[
        PIECE_TYPE_BISHOP |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ]);
}


uint8_t ChessBoard::CountRooks() const
{
    return GetNumSetBits(occupancy_bitboards[
        PIECE_TYPE_ROOK |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ]);
}


uint8_t ChessBoard::CountQueens() const
{
    return GetNumSetBits(occupancy_bitboards[
        PIECE_TYPE_QUEEN |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ]);
}


uint8_t ChessBoard::CountKings() const
{
    return GetNumSetBits(occupancy_bitboards[
        PIECE_TYPE_KING |
        (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ]);
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


