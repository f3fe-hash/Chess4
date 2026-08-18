#include <gtest/gtest.h>
#include "chess.hpp"
#include "core/bot.hpp"

#include <vector>
#include <string>


TEST(Bot, EndgameMoveTest)
{
    ChessBoard board;
    board.LoadFEN("8/3P4/8/8/8/6K1/8/7k");

    MoveResult result = bot.Search(4, 5);

    // d7 -> d8 (Pawn promotion)
    EXPECT_EQ(result.move.from, flatten_xy(3, 6));
    EXPECT_EQ(result.move.to, flatten_xy(3, 7));

    // h1 -> g1 (Only legal move)
    Move move;
    move.from = flatten_xy(7, 0);
    move.to = flatten_xy(6, 0);
    board.MakeMove(move);

    // Bot search
    result = bot.Search(4, 5);

    // d8 -> d1 (Checkmate)
    EXPECT_EQ(result.move.from, flatten_xy(3, 7));
    EXPECT_EQ(result.move.to, flatten_xy(3, 0));
}
