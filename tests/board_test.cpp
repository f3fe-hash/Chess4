#include <gtest/gtest.h>
#include "chess.hpp"

#include <algorithm>
#include <vector>
#include <string>


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
        return candidate.from == E5 && candidate.to == D6 &&
            (candidate.flags & MOVE_EN_PASSANT);
    });

    ASSERT_NE(move, moves.end());
    Move enPassantMove = *move;
    board.MakeMove(enPassantMove);
    EXPECT_EQ(board.GetPieceAt(D5), NULL_PIECE);
    EXPECT_EQ(board.GetPieceAt(D6), PIECE_TYPE_PAWN | PIECE_COLOR_WHITE);
    board.UndoMove(enPassantMove);

    EXPECT_EQ(board.GetPieceAt(E5), PIECE_TYPE_PAWN | PIECE_COLOR_WHITE);
    EXPECT_EQ(board.GetPieceAt(D5), PIECE_TYPE_PAWN | PIECE_COLOR_BLACK);
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
        if (move.from == A7 && move.to == A8)
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


