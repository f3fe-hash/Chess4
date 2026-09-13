#include <iostream>

#include "chess.hpp"
#include "core/bot.hpp"

#ifdef UCI_SERVER
# include "interface/uci.hpp"
# include "interface/uci_server.hpp"
#else
# include "interface/console.hpp"
#endif

#ifdef PGO_TEST

namespace
{

constexpr int PGO_GAMES = 10;
constexpr DurationMs PGO_TIME_LIMIT(1000);

const char* const PGO_POSITIONS[] =
{
    // Starting position
    "rnbqkbnr/pppppppp/8/8/8/8/"
    "PPPPPPPP/RNBQKBNR w KQkq - 0 1",

    // Sicilian-like position
    "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/"
    "PPPP1PPP/RNBQKBNR w KQkq - 0 2",

    // French-like position
    "rnbqkbnr/ppp1pppp/8/3p4/3P4/4P3/"
    "PPP2PPP/RNBQKBNR w KQkq - 0 2",

    // Queen's Gambit-like position
    "rnbqkbnr/pp2pppp/8/2pp4/3PP3/8/"
    "PPP2PPP/RNBQKBNR w KQkq - 0 3",

    // Open center
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/"
    "PPPP1PPP/RNBQKB1R w KQkq - 2 3",

    // Caro-Kann-like position
    "rnbqkbnr/pp2pppp/8/2pp4/4P3/2N5/"
    "PPPP1PPP/R1BQKBNR w KQkq - 1 3",

    // Ruy Lopez-like position
    "r1bqkbnr/pppp1ppp/2n5/4p3/1b2P3/2N2N2/"
    "PPPP1PPP/R1BQKB1R w KQkq - 2 4",

    // King's Indian-like position
    "rnbqk2r/pppp1pbp/5np1/8/2B1P3/2N2N2/"
    "PPPP1PPP/R1BQ1RK1 w kq - 2 6",

    // Middlegame
    "r2q1rk1/ppp1bppp/2npbn2/8/2BPP3/2N1BN2/"
    "PPP2PPP/R2Q1RK1 w - - 4 9",

    // Another middlegame
    "r1bq1rk1/ppp2ppp/2np1n2/3Np3/3NP3/2N5/"
    "PPP2PPP/R1BQ1RK1 w - - 4 9"
};

constexpr std::size_t PGO_POSITION_COUNT =
    sizeof(PGO_POSITIONS) / sizeof(PGO_POSITIONS[0]);

} // namespace

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
    UCIServer server(
        []()
        {
            auto board = std::make_shared<ChessBoard>();
            auto bot = std::make_shared<ChessBot>(board);

            return std::make_shared<UCI>(
                board,
                bot);
        },
        8080);

    server.Run();
#endif

#ifdef MATCH_TEST
    bot1 = std::make_shared<ChessBot>(board);
    bot2 = std::make_shared<ChessBot>(board);
    
    Console console(board, bot1);

    bot1->SetTimeLimit(DurationMs(1000));
    bot2->SetTimeLimit(DurationMs(1000));

    while (!(board->IsCheckMate()
        || board->IsStaleMate()
        || board->IsThreeFoldRepition()))
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

    if (board->GetTurnColor() == TURN_WHITE)
    {
        std::cout << "[BOT1] Has won (or drawn)! (black)" << std::endl;
    }
    else
    {
        std::cout << "[BOT2] Has won (or drawn)! (white)" << std::endl;
    }
#endif

#ifdef PGO_TEST

    std::cout << "Running PGO self-play test...\n";
    std::cout << "Games: " << PGO_GAMES << '\n';
    std::cout << "Time per move: "
              << PGO_TIME_LIMIT.count()
              << " ms\n\n";

    for (int game = 0; game < PGO_GAMES; ++game)
    {
        auto game_board = std::make_shared<ChessBoard>();

        const char* fen =
            PGO_POSITIONS[game % PGO_POSITION_COUNT];

        game_board->LoadFEN(fen);

        auto white_bot =
            std::make_shared<ChessBot>(game_board);

        auto black_bot =
            std::make_shared<ChessBot>(game_board);

        white_bot->SetTimeLimit(PGO_TIME_LIMIT);
        black_bot->SetTimeLimit(PGO_TIME_LIMIT);

        Console console(game_board, white_bot);

        /*
         * Alternate which bot plays which color.
         *
         * This isn't strictly necessary for PGO because both
         * bots use the same code, but it gives the test different
         * search contexts and makes the games less repetitive.
         */
        const bool reverse_colors = (game % 2) != 0;

        std::cout
            << "Game " << (game + 1)
            << "/" << PGO_GAMES
            << " - "
            << (reverse_colors
                ? "reversed colors"
                : "normal colors")
            << '\n';

        int move_count = 0;

        while (!(
            game_board->IsCheckMate()
            || game_board->IsStaleMate()
            || game_board->IsThreeFoldRepition()))
        {
            MoveResult result;

            const bool white_to_move =
                game_board->GetTurnColor() == TURN_WHITE;

            if (white_to_move)
            {
                result = reverse_colors
                    ? black_bot->Search(3, 100)
                    : white_bot->Search(3, 100);
            }
            else
            {
                result = reverse_colors
                    ? white_bot->Search(3, 100)
                    : black_bot->Search(3, 100);
            }

            game_board->MakeMove(result.move);

            ++move_count;
        }

        std::cout
            << "Game " << (game + 1)
            << " finished after "
            << move_count
            << " moves.\n";

        if (game_board->IsCheckMate())
        {
            std::cout << "Result: checkmate\n";
        }
        else if (game_board->IsStaleMate())
        {
            std::cout << "Result: stalemate\n";
        }
        else if (game_board->IsThreeFoldRepition())
        {
            std::cout << "Result: threefold repetition\n";
        }

        std::cout << '\n';
    }

    std::cout << "PGO self-play test complete.\n";

#endif

    return 0;
}


