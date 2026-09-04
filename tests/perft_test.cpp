// Performance testing

#include <gtest/gtest.h>
#include "chess.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

#include "core/transposition_table.hpp"


namespace
{

size_t Perft(ChessBoard& board, int depth)
{
    if (depth == 0)
        return 1;

    size_t nodes = 0;
    std::vector<Move> moves = board.GetLegalMoves();
    for (Move& move : moves)
    {
        board.MakeMove(move);
        nodes += Perft(board, depth - 1);
        board.UndoMove(move);
    }
    return nodes;
}


size_t TimedPerft(
    const char* position,
    ChessBoard& board,
    int depth)
{
    const auto start = std::chrono::steady_clock::now();
    const size_t nodes = Perft(board, depth);
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start);

    std::cout << position
              << " depth " << depth
              << ": " << std::fixed << std::setprecision(2)
              << elapsed.count() << " ms\n";

    return nodes;
}

}

TEST(Perft, PerftStartingPosition)
{
    ChessBoard board;
    board.LoadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    EXPECT_EQ(TimedPerft("Starting position", board, 2), 400);
    EXPECT_EQ(TimedPerft("Starting position", board, 3), 8902);
}


TEST(Perft, PerftCastlingPosition)
{
    ChessBoard board;
    board.LoadFEN("r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1");

    EXPECT_EQ(TimedPerft("Castling position", board, 1), 45);
}
