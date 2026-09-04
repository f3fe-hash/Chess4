//#define CONSOLE_APP
#define UCI_SERVER
//#define MATCH_TEST

#include <iostream>

#include "chess.hpp"
#include "core/bot.hpp"

#ifdef UCI_SERVER
# include "interface/uci.hpp"
# include "interface/uci_server.hpp"
#else
# include "interface/console.hpp"
#endif

std::shared_ptr<ChessBoard> board;
std::shared_ptr<ChessBot> bot1, bot2;


int main()
{
    board = std::make_shared<ChessBoard>();
    board->LoadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    bot1 = std::make_shared<ChessBot>(board);

#ifdef CONSOLE_APP
    Console console(board, bot1);
    bot1->SetTimeLimit(DurationMs(100));

    console.run();
#endif

#ifdef UCI_SERVER
    std::shared_ptr<UCI> uci = std::make_shared<UCI>(board, bot1);
    UCIServer server(uci, 8080);
    server.Run();
#endif

#ifdef MATCH_TEST
    bot1 = std::make_shared<ChessBot>(board);
    bot2 = std::make_shared<ChessBot>(board);
    
    Console console(board, bot1);
    bot1->SetTimeLimit(DurationMs(100));
    bot2->SetTimeLimit(DurationMs(100));

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


