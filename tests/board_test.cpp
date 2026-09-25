#include <gtest/gtest.h>
#include "chess.hpp"

#include <algorithm>
#include <memory>
#include <vector>
#include <string>

#include "core/transposition_table.hpp"


TEST(Board, StartingPositionMoveCount)
{
    ChessBoard board;

    board.LoadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    EXPECT_EQ(board.GetLegalMoves().size(), 20);
}


TEST(Board, OpeningMoveCount)
{
    ChessBoard board;

    board.LoadFEN("rnbqkb1r/ppp1pppp/5n2/1B1p4/3PP3/8/PPP2PPP/RNBQK1NR b KQkq - 0 1");

    EXPECT_EQ(board.GetLegalMoves().size(), 6);

    board.LoadFEN("rnbqkb1r/pppnpp1p/8/1B1p2p1/3PP1Q1/2N5/PPP2PPP/R1B1K1NR b KQkq - 0 1");

    EXPECT_EQ(board.GetLegalMoves().size(), 17);
}


struct PositionTest
{
    std::string fen;
    bool check;
    bool checkmate;
    bool stalemate;
};


TEST(Board, CheckDetection)
{
    const std::vector<PositionTest> tests = {
        {
            "4k3/8/8/8/8/8/4R3/4K3 b - - 0 1",
            true,
            false,
            false
        },

        {
            "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1",
            true,
            true,
            false
        },

        {
            "7k/5K2/6Q1/8/8/8/8/8 b - - 0 1",
            false,
            false,
            true
        },

        {
            "1kr5/8/Q7/8/8/8/8/7K b - - 0 1",
            false,
            false,
            false
        },

        {
            "8/8/8/8/8/5k2/5p2/5K2 b - - 0 1",
            false,
            false,
            false
        },

        {
            "k7/8/1K6/Q7/8/8/8/8 b - - 0 1",
            true,
            false,
            false
        },

        {
            "8/8/8/N2kQ3/5K2/8/8/8 b - - 0 1",
            true,
            true,
            false
        },

        {
            "1r3rk1/3q1pb1/3p1nnp/p5N1/8/PpP4P/1P1BB1P1/2KR1R2 w - - 0 1",
            false,
            false,
            false
        }
    };

    ChessBoard board;

    for (size_t idx = 0; idx < tests.size(); ++idx)
    {
        const PositionTest& test = tests[idx];

        SCOPED_TRACE("Testing position #" + std::to_string(idx));
        SCOPED_TRACE("FEN: " + test.fen);

        board.LoadFEN(test.fen);

        EXPECT_EQ(board.IsCheck(), test.check);
        EXPECT_EQ(board.IsCheckMate(), test.checkmate);
        EXPECT_EQ(board.IsStaleMate(), test.stalemate);
    }
}


TEST(Board, EnPassantAndHashRoundTrip)
{
    ChessBoard board;
    board.LoadFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");

    const uint64_t initialHash = board.GetZobristHash();
    const std::vector<Move> moves = board.GetLegalMoves();
    auto move = std::find_if(moves.begin(), moves.end(), [](const Move& candidate) {
        return candidate.from == SQ_E5 && candidate.to == SQ_D6 &&
            (candidate.flags & MOVE_EN_PASSANT);
    });

    ASSERT_NE(move, moves.end());
    Move enPassantMove = *move;
    board.MakeMove(enPassantMove);
    EXPECT_EQ(board.GetPieceAt(SQ_D5), NULL_PIECE);
    EXPECT_EQ(board.GetPieceAt(SQ_D6), PIECE_TYPE_PAWN | PIECE_COLOR_WHITE);
    board.UndoMove(enPassantMove);

    EXPECT_EQ(board.GetPieceAt(SQ_E5), PIECE_TYPE_PAWN | PIECE_COLOR_WHITE);
    EXPECT_EQ(board.GetPieceAt(SQ_D5), PIECE_TYPE_PAWN | PIECE_COLOR_BLACK);
    EXPECT_EQ(board.GetZobristHash(), initialHash);
}


TEST(Board, PromotionGeneratesAllChoices)
{
    ChessBoard board;
    board.LoadFEN("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");

    const std::vector<Move> moves = board.GetLegalMoves();
    size_t promotions = 0;
    for (const Move& move : moves)
    {
        if (move.from == SQ_A7 && move.to == SQ_A8)
            ++promotions;
    }

    EXPECT_EQ(promotions, 4);
}


TEST(Board, CastlingRightsAreLoaded)
{
    ChessBoard board;
    board.LoadFEN("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1");

    const std::vector<Move> moves = board.GetLegalMoves();
    EXPECT_TRUE(std::any_of(moves.begin(), moves.end(), [](const Move& move) {
        return move.flags & MOVE_CASTLE_KINGSIDE;
    }));
    EXPECT_TRUE(std::any_of(moves.begin(), moves.end(), [](const Move& move) {
        return move.flags & MOVE_CASTLE_QUEENSIDE;
    }));
}


TEST(Board, PinnedPieceCannotExposeKing)
{
    ChessBoard board;
    board.LoadFEN("4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1");

    const std::vector<Move> moves = board.GetLegalMoves();
    EXPECT_TRUE(std::none_of(moves.begin(), moves.end(), [](const Move& move) {
        return move.from == SQ_E2 && get_piece_x(move.to) != get_piece_x(move.from);
    }));
}


TEST(Board, RepetitionIncludesInitialPosition)
{
    ChessBoard board;
    board.LoadFEN("1n2k3/8/8/8/8/8/8/N3K3 w - - 0 1");

    auto play = [&](Square from, Square to) {
        for (Move move : board.GetLegalMoves())
        {
            if (move.from == from && move.to == to)
            {
                board.MakeMove(move);
                return;
            }
        }
        FAIL() << "Move was not legal";
    };

    play(SQ_A1, SQ_C2);
    play(SQ_B8, SQ_C6);
    play(SQ_C2, SQ_A1);
    play(SQ_C6, SQ_B8);
    play(SQ_A1, SQ_C2);
    play(SQ_B8, SQ_C6);
    play(SQ_C2, SQ_A1);
    play(SQ_C6, SQ_B8);

    EXPECT_TRUE(board.IsThreeFoldRepition());
}


// ============================================================
// Move-generation helpers
// ============================================================

static bool HasMove(
    const std::vector<Move>& moves,
    Square from,
    Square to)
{
    return std::any_of(
        moves.begin(),
        moves.end(),
        [from, to](const Move& move)
        {
            return move.from == from &&
                   move.to == to;
        });
}


static int CountMoves(
    const std::vector<Move>& moves,
    Square from,
    Square to)
{
    return static_cast<int>(std::count_if(
        moves.begin(),
        moves.end(),
        [from, to](const Move& move)
        {
            return move.from == from &&
                   move.to == to;
        }));
}


static const Move* FindMove(
    const std::vector<Move>& moves,
    Square from,
    Square to)
{
    auto it = std::find_if(
        moves.begin(),
        moves.end(),
        [from, to](const Move& move)
        {
            return move.from == from &&
                   move.to == to;
        });

    return it == moves.end() ? nullptr : &*it;
}


// ============================================================
// Basic piece move generation
// ============================================================

TEST(Board, PawnGeneratesSingleAndDoublePush)
{
    ChessBoard board;
    board.LoadFEN("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_E2, SQ_E3));
    EXPECT_TRUE(HasMove(moves, SQ_E2, SQ_E4));

    EXPECT_EQ(CountMoves(moves, SQ_E2, SQ_E3), 1);
    EXPECT_EQ(CountMoves(moves, SQ_E2, SQ_E4), 1);
}


TEST(Board, PawnCannotMoveThroughPiece)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4p3/"
        "4P3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_E3));
    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_E4));
}


TEST(Board, PawnCannotDoublePushIfSquareAheadBlocked)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "4p3/"
        "8/"
        "4P3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_E4));
}


TEST(Board, PawnCapturesOnlyDiagonally)
{
    ChessBoard board;
    board.LoadFEN("4k3/8/8/8/8/3p1p2/4P3/4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_E2, SQ_D3));
    EXPECT_TRUE(HasMove(moves, SQ_E2, SQ_F3));
    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_E3));
}


TEST(Board, KnightGeneratesEightMoves)
{
    ChessBoard board;
    board.LoadFEN("4k3/8/8/3N4/8/8/8/4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_B4));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_B6));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_C3));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_C7));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_E3));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_E7));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_F4));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_F6));

    EXPECT_EQ(CountMoves(moves, SQ_D5, SQ_B4) +
              CountMoves(moves, SQ_D5, SQ_B6) +
              CountMoves(moves, SQ_D5, SQ_C3) +
              CountMoves(moves, SQ_D5, SQ_C7) +
              CountMoves(moves, SQ_D5, SQ_E3) +
              CountMoves(moves, SQ_D5, SQ_E7) +
              CountMoves(moves, SQ_D5, SQ_F4) +
              CountMoves(moves, SQ_D5, SQ_F6), 8);
}


TEST(Board, KnightCanJumpOverPieces)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "3N4/"
        "2PPP3/"
        "2PPP3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_B3));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_B5));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_C2));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_C6));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_E2));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_E6));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_F3));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_F5));
}


TEST(Board, RookStopsAtFirstPiece)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/8/8/3p4/3R4/8/8/4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_D5));
    EXPECT_FALSE(HasMove(moves, SQ_D4, SQ_D6));
    EXPECT_FALSE(HasMove(moves, SQ_D4, SQ_D7));
}


TEST(Board, BishopStopsAtFirstPiece)
{
    ChessBoard board;

    // White bishop d4.
    // Black piece on f6 blocks the diagonal before g7.
    board.LoadFEN("4k3/8/5p2/8/3B4/8/8/4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_E5));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_F6)); // capture blocker
    EXPECT_FALSE(HasMove(moves, SQ_D4, SQ_G7));
}


TEST(Board, QueenCombinesRookAndBishopMovement)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "3Q4/"
        "8/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_D8));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_H4));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_H8));
    EXPECT_TRUE(HasMove(moves, SQ_D4, SQ_A1));
}


TEST(Board, OwnPieceCannotBeCaptured)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "3R4/"
        "3P4/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_D4, SQ_D3));
}


TEST(Board, KingGeneratesAdjacentMoves)
{
    ChessBoard board;

    board.LoadFEN(
        "8/"
        "8/"
        "8/"
        "3K4/"
        "8/"
        "8/"
        "8/"
        "4k3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_EQ(moves.size(), 8);

    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_C4));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_D4));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_E4));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_C5));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_E5));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_C6));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_D6));
    EXPECT_TRUE(HasMove(moves, SQ_D5, SQ_E6));
}


// ============================================================
// Wrong-side generation
// ============================================================

TEST(Board, GeneratesOnlyWhiteMovesWhenWhiteToMove)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4P3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    for (const Move& move : moves)
    {
        const Piece piece = board.GetPieceAt(move.from);

        EXPECT_EQ(
            piece,
            PIECE_TYPE_PAWN | PIECE_COLOR_WHITE);
    }
}


TEST(Board, GeneratesOnlyBlackMovesWhenBlackToMove)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "4p3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4K3 b - - 0 1");

    const auto moves = board.GetLegalMoves();

    for (const Move& move : moves)
    {
        const Piece piece = board.GetPieceAt(move.from);

        EXPECT_EQ(
            piece,
            PIECE_TYPE_PAWN | PIECE_COLOR_BLACK);
    }
}


// ============================================================
// Check evasions
// ============================================================

TEST(Board, KingInCheckCannotMakeNonEvasionMove)
{
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4R3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    ASSERT_FALSE(moves.empty());

    for (Move move : moves)
    {
        ChessBoard copy = board;

        copy.MakeMove(move);

        EXPECT_FALSE(copy.IsCheck());
    }
}


TEST(Board, DoubleCheckAllowsOnlyKingMoves)
{
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "8/"
        "8/"
        "5b2/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    for (const Move& move : moves)
        EXPECT_EQ(move.from, SQ_E1);
}


// ============================================================
// Pins
// ============================================================

TEST(Board, PinnedKnightCannotMove)
{
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4N3/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E3, SQ_C4));
    EXPECT_FALSE(HasMove(moves, SQ_E3, SQ_F5));
}


TEST(Board, PinnedRookCannotMoveOffFile)
{
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4R3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    for (const Move& move : moves)
    {
        if (move.from == SQ_E2)
        {
            EXPECT_EQ(
                get_piece_x(move.to),
                get_piece_x(SQ_E2));
        }
    }
}


TEST(Board, PinnedPawnCannotExposeKing)
{
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "8/"
        "4p3/"
        "8/"
        "4P3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_E3));
    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_E4));
}


// ============================================================
// King safety
// ============================================================

TEST(Board, KingCannotMoveIntoRookAttack)
{
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E1, SQ_E2));
}


TEST(Board, KingCannotCaptureProtectedPiece)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "3r4/"
        "4P3/"
        "4K3/"
        "8 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E2, SQ_D3));
}


TEST(Board, KingsCannotBecomeAdjacent)
{
    ChessBoard board;

    board.LoadFEN(
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "3k4/"
        "4K3/"
        "8 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    for (const Move& move : moves)
    {
        EXPECT_NE(move.to, SQ_D6);
        EXPECT_NE(move.to, SQ_E6);
        EXPECT_NE(move.to, SQ_F6);
    }
}


// ============================================================
// Castling
// ============================================================

TEST(Board, KingsideCastlingGenerated)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4K2R w K - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(std::any_of(
        moves.begin(),
        moves.end(),
        [](const Move& move)
        {
            return move.flags & MOVE_CASTLE_KINGSIDE;
        }));
}


TEST(Board, QueensideCastlingGenerated)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "R3K3 w Q - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_TRUE(std::any_of(
        moves.begin(),
        moves.end(),
        [](const Move& move)
        {
            return move.flags & MOVE_CASTLE_QUEENSIDE;
        }));
}


TEST(Board, CastlingNotGeneratedThroughCheck)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "5r2/"
        "8/"
        "4K2R w K - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(std::any_of(
        moves.begin(),
        moves.end(),
        [](const Move& move)
        {
            return move.flags & MOVE_CASTLE_KINGSIDE;
        }));
}


TEST(Board, CastlingNotGeneratedWhileInCheck)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4r3/"
        "4K2R w K - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(std::any_of(
        moves.begin(),
        moves.end(),
        [](const Move& move)
        {
            return move.flags & MOVE_CASTLE_KINGSIDE;
        }));
}


// ============================================================
// En passant
// ============================================================

TEST(Board, EnPassantGeneratedWhenAvailable)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "3pP3/"
        "8/"
        "8/"
        "8/"
        "4K3 w - d6 0 1");

    const auto moves = board.GetLegalMoves();

    const Move* move =
        FindMove(moves, SQ_E5, SQ_D6);

    ASSERT_NE(move, nullptr);
    EXPECT_TRUE(move->flags & MOVE_EN_PASSANT);
}


TEST(Board, EnPassantNotGeneratedWithoutTargetSquare)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "3pP3/"
        "8/"
        "8/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(HasMove(moves, SQ_E5, SQ_D6));
}


TEST(Board, PinnedEnPassantIsIllegal)
{
    // White king is on e1. The e5 pawn is shielding the king
    // from the rook on e8. En passant would remove the pawn
    // from e5 and expose the king.
    ChessBoard board;

    board.LoadFEN(
        "4r1k1/"
        "8/"
        "8/"
        "3pP3/"
        "8/"
        "8/"
        "8/"
        "4K3 w - d6 0 1");

    const auto moves = board.GetLegalMoves();

    EXPECT_FALSE(
        HasMove(moves, SQ_E5, SQ_D6));
}


// ============================================================
// Promotions
// ============================================================

TEST(Board, QuietPromotionGeneratesExactlyFourMoves)
{
    ChessBoard board;

    board.LoadFEN(
        "7k/"
        "P7/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    int promotions = 0;

    for (const Move& move : moves)
    {
        if (move.from == SQ_A7 &&
            move.to == SQ_A8)
        {
            ++promotions;
        }
    }

    EXPECT_EQ(promotions, 4);
}


TEST(Board, PromotionCaptureGeneratesFourChoices)
{
    ChessBoard board;

    board.LoadFEN(
        "1r5k/"
        "P7/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    int promotions = 0;

    for (const Move& move : moves)
    {
        if (move.from == SQ_A7 &&
            move.to == SQ_B8)
        {
            ++promotions;
        }
    }

    EXPECT_EQ(promotions, 4);
}


// ============================================================
// No duplicates
// ============================================================

TEST(Board, NoDuplicateLegalMoves)
{
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        "r3k2r/ppp2ppp/2n1bn2/3pp3/3PP3/2N1BN2/PPP2PPP/R3K2R w KQkq - 0 1",

        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",

        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1"
    };

    ChessBoard board;

    for (const std::string& fen : fens)
    {
        SCOPED_TRACE("FEN: " + fen);

        ASSERT_TRUE(board.LoadFEN(fen));

        const auto moves = board.GetLegalMoves();

        for (size_t i = 0; i < moves.size(); ++i)
        {
            for (size_t j = i + 1; j < moves.size(); ++j)
            {
                EXPECT_FALSE(
                    moves[i].from == moves[j].from &&
                    moves[i].to == moves[j].to &&
                    moves[i].promotion == moves[j].promotion &&
                    moves[i].flags == moves[j].flags)
                    << "Duplicate move found";
            }
        }
    }
}


// ============================================================
// MakeMove / UndoMove invariants for EVERY generated move
// ============================================================

TEST(Board, EveryLegalMoveRoundTrips)
{
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        "rnbqkb1r/ppp1pppp/5n2/1B1p4/3PP3/8/PPP2PPP/RNBQK1NR b KQkq - 0 1",

        "r3k2r/ppp2ppp/2n1bn2/3pp3/3PP3/2N1BN2/PPP2PPP/R3K2R w KQkq - 0 1",

        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",

        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1",

        "4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1"
    };

    ChessBoard board;

    for (const std::string& fen : fens)
    {
        SCOPED_TRACE("FEN: " + fen);

        ASSERT_TRUE(board.LoadFEN(fen));

        const uint64_t initialHash =
            board.GetZobristHash();

        const auto initialMoves =
            board.GetLegalMoves();

        for (const Move& move : initialMoves)
        {
            Move move_copy = move;
            board.MakeMove(move_copy);
            board.UndoMove(move_copy);

            EXPECT_EQ(
                board.GetZobristHash(),
                initialHash);

            const auto movesAfterUndo =
                board.GetLegalMoves();

            EXPECT_EQ(
                movesAfterUndo.size(),
                initialMoves.size());
        }
    }
}


// ============================================================
// GetLegalMoves itself must not modify the position
// ============================================================

TEST(Board, GetLegalMovesDoesNotChangePosition)
{
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        "rnbqkb1r/ppp1pppp/5n2/1B1p4/3PP3/8/PPP2PPP/RNBQK1NR b KQkq - 0 1",

        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",

        "4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1"
    };

    ChessBoard board;

    for (const std::string& fen : fens)
    {
        SCOPED_TRACE("FEN: " + fen);

        ASSERT_TRUE(board.LoadFEN(fen));

        const uint64_t before =
            board.GetZobristHash();

        const auto moves1 =
            board.GetLegalMoves();

        const uint64_t after =
            board.GetZobristHash();

        EXPECT_EQ(before, after);

        const auto moves2 =
            board.GetLegalMoves();

        EXPECT_EQ(moves1.size(), moves2.size());
    }
}


// ============================================================
// Side-to-move must alternate correctly after moves
// ============================================================

TEST(Board, LegalMoveChangesSideToMove)
{
    ChessBoard board;

    board.LoadFEN(
        "4k3/"
        "8/"
        "8/"
        "8/"
        "8/"
        "8/"
        "4P3/"
        "4K3 w - - 0 1");

    const auto moves = board.GetLegalMoves();

    ASSERT_TRUE(HasMove(
        moves,
        SQ_E2,
        SQ_E4));

    const Move* move =
        FindMove(moves, SQ_E2, SQ_E4);

    ASSERT_NE(move, nullptr);

    Move move_copy = *move;
    board.MakeMove(move_copy);

    // After White moves, Black must be to move.
    const auto blackMoves =
        board.GetLegalMoves();

    for (const Move& blackMove : blackMoves)
    {
        const Piece piece =
            board.GetPieceAt(blackMove.from);

        EXPECT_EQ(
            piece,
            PIECE_TYPE_KING | PIECE_COLOR_BLACK);
    }
}


// ============================================================
// Generated moves must always originate from occupied squares
// of the side to move.
// ============================================================

TEST(Board, EveryGeneratedMoveHasCorrectMover)
{
    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        "rnbqkb1r/ppp1pppp/5n2/1B1p4/3PP3/8/PPP2PPP/RNBQK1NR b KQkq - 0 1",

        "4k3/8/8/8/8/8/8/4K3 w - - 0 1",

        "4k3/8/8/8/8/8/8/4K3 b - - 0 1"
    };

    ChessBoard board;

    for (const std::string& fen : fens)
    {
        SCOPED_TRACE("FEN: " + fen);

        ASSERT_TRUE(board.LoadFEN(fen));

        const auto moves =
            board.GetLegalMoves();

        const Piece expectedColor =
            board.GetTurnColor() == TURN_WHITE
                ? PIECE_COLOR_WHITE
                : PIECE_COLOR_BLACK;

        for (const Move& move : moves)
        {
            const Piece piece =
                board.GetPieceAt(move.from);

            EXPECT_NE(piece, NULL_PIECE);

            EXPECT_EQ(
                get_piece_color(piece),
                expectedColor);
        }
    }
}


// ============================================================
// Legal move count sanity / known perft positions
// ============================================================

TEST(Board, StartPositionPerftDepthOne)
{
    ChessBoard board;

    board.LoadFEN(
        "rnbqkbnr/"
        "pppppppp/"
        "8/"
        "8/"
        "8/"
        "8/"
        "PPPPPPPP/"
        "RNBQKBNR w KQkq - 0 1");

    EXPECT_EQ(board.GetLegalMoves().size(), 20);
}


TEST(Board, StartPositionPerftDepthTwo)
{
    ChessBoard board;

    board.LoadFEN(
        "rnbqkbnr/"
        "pppppppp/"
        "8/"
        "8/"
        "8/"
        "8/"
        "PPPPPPPP/"
        "RNBQKBNR w KQkq - 0 1");

    size_t nodes = 0;

    for (const Move& move : board.GetLegalMoves())
    {
        Move move_copy = move;
        board.MakeMove(move_copy);

        nodes += board.GetLegalMoves().size();

        board.UndoMove(move_copy);
    }

    EXPECT_EQ(nodes, 400);
}


TEST(Board, KiwipetePerftDepthOne)
{
    ChessBoard board;
    board.LoadFEN(
        "r3k2r/p1ppqpb1/bn2pnp1/2pP4/"
        "1p2P3/2N2N2/PPQBBPPP/R3K2R "
        "w KQkq - 0 1"
    );

    EXPECT_EQ(board.GetLegalMoves().size(), 48);
}


TEST(Board, KiwipetePerftDepthTwo)
{
    ChessBoard board;

    board.LoadFEN(
        "r3k2r/p1ppqpb1/bn2pnp1/2pP4/"
        "1p2P3/2N2N2/PPQBBPPP/R3K2R "
        "w KQkq - 0 1"
    );

    size_t nodes = 0;

    for (const Move& move : board.GetLegalMoves())
    {
        Move move_copy = move;
        board.MakeMove(move_copy);

        nodes += board.GetLegalMoves().size();

        board.UndoMove(move_copy);
    }

    EXPECT_EQ(nodes, 2039);
}


// ============================================================
// Repeated generation must be deterministic
// ============================================================

TEST(Board, MoveGenerationIsDeterministic)
{
    ChessBoard board;

    board.LoadFEN(
        "r3k2r/"
        "p1ppqpb1/"
        "bn2pnp1/"
        "2pP4/"
        "1p2P3/"
        "2N2N2/"
        "PPQBBPPP/"
        "R3K2R w KQkq - 0 1");

    const auto first =
        board.GetLegalMoves();

    for (int i = 0; i < 100; ++i)
    {
        const auto current =
            board.GetLegalMoves();

        ASSERT_EQ(current.size(), first.size());

        for (size_t j = 0; j < first.size(); ++j)
        {
            EXPECT_EQ(current[j].from, first[j].from);
            EXPECT_EQ(current[j].to, first[j].to);
            EXPECT_EQ(current[j].promotion, first[j].promotion);
            EXPECT_EQ(current[j].flags, first[j].flags);
        }
    }
}