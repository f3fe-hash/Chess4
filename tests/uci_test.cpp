#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "interface/uci.hpp"


namespace
{

std::unique_ptr<UCI> MakeUCI()
{
    auto board = std::make_shared<ChessBoard>();
    board->LoadFEN(
        "rnbqkbnr/pppppppp/8/8/8/8/"
        "PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    auto bot = std::make_shared<ChessBot>(board);
    return std::make_unique<UCI>(board, bot);
}

}


TEST(UCI, HandshakeAndReady)
{
    auto uci = MakeUCI();

    const std::string handshake = uci->Respond("uci");
    EXPECT_NE(handshake.find("id name FastKat"), std::string::npos);
    EXPECT_NE(handshake.find("uciok"), std::string::npos);
    EXPECT_EQ(uci->Respond("isready"), "readyok\n");
}


TEST(UCI, PositionAppliesMoves)
{
    auto uci = MakeUCI();

    EXPECT_EQ(uci->Respond("position startpos moves e2e4 e7e5"), "");
    EXPECT_EQ(uci->Respond("setoption name Hash value 128"), "");
    EXPECT_EQ(uci->Respond("ponderhit"),
              "info string ponderhit without ponder search\n");
}


TEST(UCI, GoStopReturnsBestMove)
{
    auto uci = MakeUCI();

    EXPECT_EQ(uci->Respond("go depth 2"), "");
    const std::string response = uci->Respond("stop");
    EXPECT_NE(response.find("bestmove "), std::string::npos);
    EXPECT_FALSE(uci->IsSearching());
}


TEST(UCI, PonderHitStopsPonderBudget)
{
    auto uci = MakeUCI();

    EXPECT_EQ(uci->Respond("go ponder depth 100"), "");
    EXPECT_EQ(uci->Respond("ponderhit"), "");
    const std::string response = uci->Respond("stop");
    EXPECT_NE(response.find("bestmove "), std::string::npos);
}
