#include <iostream>

#include "chess.hpp"
#include "bot.hpp"

int main()
{
    std::shared_ptr<ChessBoard> board;
    ChessBot bot(board);

    return 0;
}