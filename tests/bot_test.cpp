#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>

#include "core/bot.hpp"



TEST(Bot, SearchIsRepeatableOnTacticalPosition)
{
    auto board = std::make_shared<ChessBoard>();
    board->LoadFEN("4k3/8/8/3q4/8/8/4R3/4K3 w - - 0 1");
    ChessBot bot(board);
    bot.SetTimeLimit(DurationMs(0));

    const MoveResult first = bot.Search(3, 3);
    ASSERT_TRUE(board->IsLegalMove(first.move));

    for (int attempt = 0; attempt < 5; ++attempt)
    {
        const MoveResult result = bot.Search(3, 3);
        EXPECT_EQ(result.move.from, first.move.from);
        EXPECT_EQ(result.move.to, first.move.to);
        EXPECT_EQ(result.move.promotion, first.move.promotion);
    }
}


TEST(Bot, BlackSearchTakesFreeQueen)
{
    auto board = std::make_shared<ChessBoard>();
    board->LoadFEN("3rk3/8/8/8/3Q4/8/8/4K3 b - - 0 1");
    ChessBot bot(board);
    bot.SetTimeLimit(DurationMs(0));

    const MoveResult result = bot.Search(2, 2);

    ASSERT_TRUE(board->IsLegalMove(result.move));
    EXPECT_EQ(result.move.from, D8);
    EXPECT_EQ(result.move.to, D4);
    EXPECT_EQ(result.mate_in_ply, -2);
}


TEST(Bot, BlackFindsMateInOneAndGetsMateScore)
{
    auto board = std::make_shared<ChessBoard>();
    board->LoadFEN("6r1/8/8/8/8/8/4q3/7K b - - 0 1");

    Move mate_move{};
    for (const Move move : board->GetLegalMoves())
    {
        if (move.from == E2 && move.to == G2)
        {
            mate_move = move;
            break;
        }
    }
    ASSERT_EQ(mate_move.from, E2);
    ASSERT_EQ(mate_move.to, G2);
    board->MakeMove(mate_move);
    EXPECT_TRUE(board->IsCheckMate());
    board->UndoMove(mate_move);

    ChessBot bot(board);
    bot.SetTimeLimit(DurationMs(0));

    const MoveResult result = bot.Search(1, 1);

    ASSERT_TRUE(board->IsLegalMove(result.move));
    EXPECT_EQ(result.move.from, E2);
    EXPECT_EQ(result.move.to, G2);
    EXPECT_EQ(result.mate_in_ply, 1);
    EXPECT_LT(result.eval, -CHECKMATE_SCORE + 10);
}


TEST(Bot, DoesNotPlayQueenIntoKingCapture)
{
    auto board = std::make_shared<ChessBoard>();
    board->LoadFEN("r1b1kb1r/pp2pppp/2pq4/8/B2P3/1P6/P1P2PPN/R1BQR1K1 b kq - 0 14");
    ChessBot bot(board);
    bot.SetTimeLimit(DurationMs(0));

    Move queenMove{};
    for (Move move : board->GetLegalMoves())
    {
        if (move.from == D6 && move.to == H2)
        {
            queenMove = move;
            break;
        }
    }
    ASSERT_NE(queenMove.moved, NULL_PIECE);
    board->MakeMove(queenMove);
    const std::vector<Move> responses = board->GetLegalMoves();
    EXPECT_TRUE(std::any_of(
        responses.begin(), responses.end(),
        [](const Move& move) { return move.from == G1 && move.to == H2; }));
    Move kingCapture = *std::find_if(responses.begin(), responses.end(), [](const Move& move) {
        return move.from == G1 && move.to == H2;
    });
    board->MakeMove(kingCapture);
    const Evaluation afterCapture = bot.EvaluateRaw();
    board->UndoMove(kingCapture);
    board->UndoMove(queenMove);

    for (int depth = 1; depth <= 3; ++depth)
    {
        const MoveResult result = bot.Search(depth, depth);
        ASSERT_TRUE(board->IsLegalMove(result.move));
        EXPECT_FALSE(result.move.from == D6 && result.move.to == H2);
    }
    EXPECT_GT(afterCapture, -500);
}
