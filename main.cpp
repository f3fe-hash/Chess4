#include <iostream>

#include "chess.hpp"
#include "console.hpp"
#include "core/bot.hpp"

int main()
{
    auto board = std::make_shared<ChessBoard>();
    board->LoadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto bot = std::make_shared<ChessBot>(board);

    Console console(board, bot);
    console.run();

    return 0;
}