#include <iostream>

#include "chess.hpp"
#include "bot.hpp"
#include "console.hpp"

int main()
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<ChessBot> bot = std::make_shared<ChessBot>(board);

    Console console(board, bot);
    console.run();

    return 0;
}