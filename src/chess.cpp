#include "chess.hpp"

namespace
{

static Bitboard pawn_attack_lookup[2][64];
static Bitboard knight_attack_lookup[64];
static Bitboard king_attack_lookup[64];
static Bitboard bishop_rays[64];
static Bitboard rook_rays[64];
static Bitboard queen_rays[64];
static bool attack_lookup_initialized = false;

inline Bitboard SquareMask(Square square)
{
    return Bitboard(1ULL) << square;
}

inline bool IsOnBoard(int x, int y)
{
    return (x >= 0) && (x < 8) && (y >= 0) && (y < 8);
}

inline Square FlattenSquare(int x, int y)
{
    return Square(flatten_xy(x, y));
}

inline bool PieceIsFriendly(Piece piece, TurnColor color)
{
    if (piece == NULL_PIECE)
        return false;
    return (color == TURN_WHITE) ? bool(piece & PIECE_COLOR_WHITE) : bool(piece & PIECE_COLOR_BLACK);
}

inline bool PieceIsOpponent(Piece piece, TurnColor color)
{
    if (piece == NULL_PIECE)
        return false;
    return !PieceIsFriendly(piece, color);
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
        return Square(65);

    return PopLeastSignificantBit(bits);
}

inline void AddMove(std::vector<Move>& moves, Square from, Square to, Piece moved, Piece captured)
{
    Move move;
    move.from = from;
    move.to = to;
    move.moved = moved;
    move.captured = captured;
    move.flags = 0;
    moves.push_back(move);
}

Bitboard ComputeSlidingAttacks(Square square, Bitboard occupancy, const int directions[][2], int directionCount)
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
}


Piece ChessBoard::GetPieceAt(Square square) const
{
    if (square < 64)
        return pieces[square];
    return NULL_PIECE;
}


bool ChessBoard::LoadFEN(const std::string& fen)
{
    // Clear board
    for (int i = 0; i < 64; ++i)
        pieces[i] = NULL_PIECE;

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

    // Update occupancies and attacks
    UpdateAttackBitboards();

    return true;
}


ChessBoard::~ChessBoard()
{}


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

    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;

    attack_bitboard_white = 0ULL;
    attack_bitboard_black = 0ULL;

    for (int i = 0; i < 32; ++i)
        attack_bitboards[i] = 0ULL;

    for (Square square = 0; square < 64; ++square)
    {
        Piece piece = pieces[square];
        if (piece == NULL_PIECE)
            continue;

        Bitboard attacks = 0ULL;
        Piece pieceType = Piece(piece & 0x07);

static const int bishopDirs[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    static const int rookDirs[4][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };
    static const int queenDirs[8][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    switch (pieceType)
    {
        case PIECE_TYPE_PAWN:
            if (piece & PIECE_COLOR_WHITE)
                attacks = pawn_attack_lookup[0][square];
            else
                attacks = pawn_attack_lookup[1][square];
            break;

        case PIECE_TYPE_KNIGHT:
            attacks = knight_attack_lookup[square];
            break;

        case PIECE_TYPE_BISHOP:
            attacks = ComputeSlidingAttacks(square, occupancy, bishopDirs, 4);
            break;

        case PIECE_TYPE_ROOK:
            attacks = ComputeSlidingAttacks(square, occupancy, rookDirs, 4);
            break;

        case PIECE_TYPE_QUEEN:
            attacks = ComputeSlidingAttacks(square, occupancy, queenDirs, 8);
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
        else if (piece & PIECE_COLOR_BLACK)
            attack_bitboard_black |= attacks;
    }
}


// Computes attack lookup bitboards for all pieces, so generating
// attacks becomes looking in an array, and seeing where pieces can
// actually move.
void ChessBoard::ComputeAttackLookupBitboards()
{
    if (attack_lookup_initialized)
        return;

    const int knightOffsets[8][2] = {
        {1, 2}, {2, 1}, {2, -1}, {1, -2},
        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}
    };

    const int kingOffsets[8][2] = {
        {1, 0}, {1, 1}, {0, 1}, {-1, 1},
        {-1, 0}, {-1, -1}, {0, -1}, {1, -1}
    };

    const int bishopDirs[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    const int rookDirs[4][2] = {
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
    }

    attack_lookup_initialized = true;
}


void ChessBoard::MakeMove(Move& move)
{
    if (move.moved == NULL_PIECE)
        move.moved = pieces[move.from];
    
    if (move.captured == NULL_PIECE)
        move.captured = pieces[move.to];

    Piece movedPiece = move.moved;
    Piece capturedPiece = move.captured;

    // Promotion
    if ((movedPiece & 0x07) == PIECE_TYPE_PAWN)
    {
        int fromY = get_piece_y(move.from);
        int toY = get_piece_y(move.to);
        if ((turn == TURN_WHITE && fromY == 6 && toY == 7) ||
            (turn == TURN_BLACK && fromY == 1 && toY == 0))
        {
            move.flags &= ~0x07;
            move.flags |= PIECE_TYPE_QUEEN;
            movedPiece = PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK);
        }
    }

    pieces[move.from] = NULL_PIECE;
    pieces[move.to] = movedPiece;

    Bitboard fromMask = SquareMask(move.from);
    Bitboard toMask = SquareMask(move.to);
    bool movingWhite = (turn == TURN_WHITE);

    if (movingWhite)
    {
        occupancy_bitboard_white &= ~fromMask;
        occupancy_bitboard_white |= toMask;
        if (capturedPiece != NULL_PIECE)
            occupancy_bitboard_black &= ~toMask;
    }
    else
    {
        occupancy_bitboard_black &= ~fromMask;
        occupancy_bitboard_black |= toMask;
        if (capturedPiece != NULL_PIECE)
            occupancy_bitboard_white &= ~toMask;
    }

    occupancy_bitboards[movedPiece] &= ~fromMask;
    occupancy_bitboards[movedPiece] |= toMask;

    if (capturedPiece != NULL_PIECE)
        occupancy_bitboards[capturedPiece] &= ~toMask;

    move.moved = movedPiece;
    move.captured = capturedPiece;

    turn ^= 1;

    UpdateAttackBitboards();
}


void ChessBoard::UndoMove(Move move)
{
    bool movingWhite = (turn == TURN_BLACK);
    Piece movedPiece = move.moved;
    Piece restoredPiece = movedPiece;
    Piece pawnPiece = PIECE_TYPE_PAWN | (movingWhite ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK);

    if (move.flags & 0x07)
    {
        restoredPiece = pawnPiece;
    }

    pieces[move.from] = restoredPiece;
    pieces[move.to] = move.captured;

    Bitboard fromMask = SquareMask(move.from);
    Bitboard toMask = SquareMask(move.to);

    if (movingWhite)
    {
        occupancy_bitboard_white |= fromMask;
        occupancy_bitboard_white &= ~toMask;
        if (move.captured != NULL_PIECE)
            occupancy_bitboard_black |= toMask;
    }
    else
    {
        occupancy_bitboard_black |= fromMask;
        occupancy_bitboard_black &= ~toMask;
        if (move.captured != NULL_PIECE)
            occupancy_bitboard_white |= toMask;
    }

    occupancy_bitboards[movedPiece] &= ~toMask;
    occupancy_bitboards[restoredPiece] |= fromMask;

    if (move.captured != NULL_PIECE)
        occupancy_bitboards[move.captured] |= toMask;

    turn ^= 1;

    UpdateAttackBitboards();
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
            AddMove(moves, square, to, piece, pieces[to]);
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
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
}


void ChessBoard::GetLegalBishopAttacks(std::vector<Move>& moves)
{
    
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;

    static const int bishopDirs[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard bishops = occupancy_bitboards[
        PIECE_TYPE_BISHOP | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (bishops)
    {
        Square square = PopLeastSignificantBit(bishops);
        Piece piece = pieces[square];
        Bitboard targets = ComputeSlidingAttacks(square, occupancy, bishopDirs, 4) & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
}


void ChessBoard::GetLegalQueenAttacks(std::vector<Move>& moves)
{
    
    Bitboard opponentOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_black : occupancy_bitboard_white;

    static const int queenDirs[8][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard queens = occupancy_bitboards[
        PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (queens)
    {
        Square square = PopLeastSignificantBit(queens);
        Piece piece = pieces[square];
        Bitboard targets = ComputeSlidingAttacks(square, occupancy, queenDirs, 8) & opponentOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to]);
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
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
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
            AddMove(moves, square, singleTarget, piece, pieces[singleTarget]);

            if (y == startRank)
            {
                int doubleTargetY = y + (forward * 2);
                if (IsOnBoard(x, doubleTargetY))
                {
                    Square doubleTarget = FlattenSquare(x, doubleTargetY);
                    Bitboard doubleMask = SquareMask(doubleTarget);
                    if (!(occupancy & doubleMask))
                        AddMove(moves, square, doubleTarget, piece, pieces[doubleTarget]);
                }
            }
        }

        Bitboard captureTargets = pawn_attack_lookup[(turn == TURN_WHITE) ? 0 : 1][square] & ~friendlyOccupancy;
        while (captureTargets)
        {
            Square to = PopLeastSignificantBit(captureTargets);
            if (occupancy & SquareMask(to))
                AddMove(moves, square, to, piece, pieces[to]);
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
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
}


void ChessBoard::GetLegalBishopMoves(std::vector<Move>& moves)
{
    
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;

    static const int bishopDirs[4][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    Bitboard bishops = occupancy_bitboards[
        PIECE_TYPE_BISHOP | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (bishops)
    {
        Square square = PopLeastSignificantBit(bishops);
        Piece piece = pieces[square];
        Bitboard targets = ComputeSlidingAttacks(square, occupancy, bishopDirs, 4) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
}


void ChessBoard::GetLegalQueenMoves(std::vector<Move>& moves)
{
    
    Bitboard occupancy = occupancy_bitboard_white | occupancy_bitboard_black;
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;

    static const int queenDirs[8][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };

    Bitboard queens = occupancy_bitboards[
        PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (queens)
    {
        Square square = PopLeastSignificantBit(queens);
        Piece piece = pieces[square];
        Bitboard targets = ComputeSlidingAttacks(square, occupancy, queenDirs, 8) & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
}


void ChessBoard::GetLegalKingMoves(std::vector<Move>& moves)
{
    
    Bitboard friendlyOccupancy = (turn == TURN_WHITE) ? occupancy_bitboard_white : occupancy_bitboard_black;
    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    while (kings)
    {
        Square square = PopLeastSignificantBit(kings);
        Piece piece = pieces[square];
        Bitboard targets = king_attack_lookup[square] & ~friendlyOccupancy;
        while (targets)
        {
            Square to = PopLeastSignificantBit(targets);
            AddMove(moves, square, to, piece, pieces[to]);
        }
    }
}


std::vector<Move> ChessBoard::GetLegalMoves()
{
    std::vector<Move> moves;
    GetLegalPawnMoves(moves);
    GetLegalKnightMoves(moves);
    GetLegalBishopMoves(moves);
    GetLegalQueenMoves(moves);
    GetLegalKingMoves(moves);
    
    std::vector<Move> legal_moves;
    legal_moves.reserve(moves.size());

    for (Move move : moves)
    {
        MakeMove(move);
        bool in_check = IsCheck();
        UndoMove(move);

        if (in_check)
            continue;

        legal_moves.push_back(move);
    }

    return legal_moves;
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


uint8_t ChessBoard::CountPawns()
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


uint8_t ChessBoard::CountKnights()
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


uint8_t ChessBoard::CountBishops()
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


uint8_t ChessBoard::CountRooks()
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


uint8_t ChessBoard::CountQueens()
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


uint8_t ChessBoard::CountKings()
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


Square ChessBoard::PopPawns()
{
    Bitboard pawns = occupancy_bitboards[
        PIECE_TYPE_PAWN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(pawns);
}


Square ChessBoard::PopKnights()
{
    Bitboard knights = occupancy_bitboards[
        PIECE_TYPE_KNIGHT | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(knights);
}


Square ChessBoard::PopBishops()
{
    Bitboard bishops = occupancy_bitboards[
        PIECE_TYPE_BISHOP | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(bishops);
}


Square ChessBoard::PopRooks()
{
    Bitboard rooks = occupancy_bitboards[
        PIECE_TYPE_ROOK | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(rooks);
}


Square ChessBoard::PopQueens()
{
    Bitboard queens = occupancy_bitboards[
        PIECE_TYPE_QUEEN | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(queens);
}


Square ChessBoard::PopKings()
{
    Bitboard kings = occupancy_bitboards[
        PIECE_TYPE_KING | (turn == TURN_WHITE ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK)
    ];

    return PopBitboard(kings);
}

