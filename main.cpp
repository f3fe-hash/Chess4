//#define CONSOLE_APP
#define MATCH_TEST

#include <iostream>

#include "chess.hpp"
#include "console.hpp"
#include "core/bot.hpp"

std::shared_ptr<ChessBoard> board;
std::shared_ptr<ChessBot> bot1, bot2;


int main()
{
    board = std::make_shared<ChessBoard>();

    bot1 = std::make_shared<ChessBot>(board);
    bot2 = std::make_shared<ChessBot>(board);
    
    Console console(board, bot1);
    bot1->SetTimeLimit(DurationMs(10000));
    bot2->SetTimeLimit(DurationMs(10000));

    board->LoadFEN("rnbqkbnr/pppppp1p/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
#ifdef CONSOLE_APP
    console.run();
#endif

#ifdef MATCH_TEST
    console.PrintBoard();
    while (!(board->IsCheckMate() || board->IsStaleMate() || board->IsThreeFoldRepition()))
    {
        MoveResult result;
        if (board->GetTurnColor() == TURN_WHITE)
        {
            result = bot1->Search(3, 100);
        }
        else
        {
            result = bot2->Search(3, 100);
        }

        board->MakeMove(result.move);

        if (board->GetTurnColor() == TURN_WHITE)
        {
            std::cout << "[BOT1] has made the move ";
        }
        else
        {
            std::cout << "[BOT2] has made the move ";
        }

        std::cout << console.MoveToString(result.move) << "." << std::endl;
    }

    console.PrintBoard();
    console.PrintEndgame();

    if (board->GetTurnColor() == TURN_WHITE)
    {
        std::cout << "[BOT1] Has won (or drawn)! (black)" << std::endl;
    }
    else
    {
        std::cout << "[BOT2] Has won (or drawn)! (white)" << std::endl;
    }
#endif

    return 0;
}


