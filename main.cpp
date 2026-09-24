// Only prints a game summary if there is less
// than MATCH_TEST_NOTIFICATION_MOVES moves in a game.
#define MATCH_TEST_NOTIFY_LOW_MOVES

#define MATCH_TEST_NOTIFICATION_MOVES 15

#include <iostream>

#include "chess.hpp"
#include "core/bot.hpp"

#ifdef UCI_SERVER
# include "interface/uci.hpp"
# include "interface/uci_server.hpp"
#else
# include "interface/console.hpp"
#endif

// MATCH_TEST is now an alias for PGO_TEST.
// This keeps the old build option working without maintaining
// a separate match-test implementation.
#ifdef MATCH_TEST
#define PGO_TEST
#endif

#ifdef PGO_TEST

#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>

constexpr const char* PGO_FEN_FILE = "games.fen";

// How many games to play from the PGO.
constexpr const std::size_t PLAY_GAMES = 100;

// ms
constexpr const DurationMs PGO_TIME_LIMIT = DurationMs(100);

std::vector<std::string> LoadPGOPositions()
{
    std::ifstream file(PGO_FEN_FILE);

    if (!file)
    {
        throw std::runtime_error(
            std::string("Could not open PGO FEN file: ")
            + PGO_FEN_FILE);
    }

    std::vector<std::string> positions;
    std::string fen;

    while (std::getline(file, fen))
    {
        if (!fen.empty())
        {
            positions.push_back(fen);
        }
    }

    return positions;
}

#endif


std::shared_ptr<ChessBoard> board;
std::shared_ptr<ChessBot> bot1, bot2;


int main()
{
    board = std::make_shared<ChessBoard>();

    board->LoadFEN(
        "rnbqkbnr/pppppppp/8/8/8/8/"
        "PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    );

    bot1 = std::make_shared<ChessBot>(board);

#ifdef CONSOLE_APP

    Console console(board, bot1);

    bot1->SetTimeLimit(
        DurationMs(100));

    console.run();

#endif


#ifdef UCI_SERVER

    UCIServer server(
        []()
        {
            auto board =
                std::make_shared<ChessBoard>();

            auto bot =
                std::make_shared<ChessBot>(board);

            return std::make_shared<UCI>(
                board,
                bot);
        },
        8080);

    server.Run();

#endif


#ifdef PGO_TEST

    std::vector<std::string> pgo_positions;

    try
    {
        pgo_positions = LoadPGOPositions();
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "PGO ERROR: "
            << e.what()
            << '\n';

        return 1;
    }

    if (pgo_positions.empty())
    {
        std::cerr
            << "PGO ERROR: "
            << PGO_FEN_FILE
            << " contains no positions.\n";

        return 1;
    }

    const std::size_t NUM_GAMES =
        std::min(
            pgo_positions.size(),
            PLAY_GAMES);

    std::cout
        << "Running PGO self-play test...\n";

    std::cout
        << "Positions: "
        << NUM_GAMES
        << '\n';

    std::cout
        << "Time per move: "
        << PGO_TIME_LIMIT.count()
        << " ms\n\n";

    auto game_board =
        std::make_shared<ChessBoard>();

    int white_wins = 0;
    int black_wins = 0;
    int draws = 0;

    int white_games = 0;
    int black_games = 0;

    for (std::size_t game = 0;
        game < NUM_GAMES;
        ++game)
    {
        const std::string& fen =
            pgo_positions[game];

        game_board->LoadFEN(fen);

        auto white_bot =
            std::make_shared<ChessBot>(
                game_board);

        auto black_bot =
            std::make_shared<ChessBot>(
                game_board);

        white_bot->SetTimeLimit(
            PGO_TIME_LIMIT);

        black_bot->SetTimeLimit(
            PGO_TIME_LIMIT);

        /*
        * Alternate colors for the starting position.
        *
        * This is important because the FEN itself determines
        * whose turn it is, while the bots are otherwise identical.
        */
        const bool reverse_colors =
            (game % 2) != 0;

        if (reverse_colors)
        {
            ++black_games;
        }
        else
        {
            ++white_games;
        }

        int move_count = 0;

        /*
        * Safety limit so a pathological position cannot
        * make the PGO run forever.
        */
        constexpr int MAX_GAME_MOVES = 300;

        while (
            !game_board->IsCheckMate()
            && !game_board->IsStaleMate()
            && !game_board->IsThreeFoldRepition()
            && move_count < MAX_GAME_MOVES)
        {
            MoveResult result;

            const bool white_to_move =
                game_board->GetTurnColor()
                == TURN_WHITE;

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

        /*
        * Only notify about games that finish below the
        * configured move threshold when the notification
        * macro is enabled.
        */
    #ifdef MATCH_TEST_NOTIFY_LOW_MOVES
        const bool notify_game =
            move_count < MATCH_TEST_NOTIFICATION_MOVES;
    #else
        constexpr bool notify_game = true;
    #endif

        /*
        * Print everything related to this game only when
        * notification is enabled for this game.
        */
        if (notify_game)
        {
            std::cout
                << "Game "
                << (game + 1)
                << "/"
                << NUM_GAMES
                << " - "
                << (reverse_colors
                    ? "reversed colors"
                    : "normal colors")
                << '\n';
        }

        /*
        * The moves have already been played above.
        *
        * DEBUG output needs to happen during the game, so
        * if DEBUG is enabled and notifications are disabled,
        * replaying the moves is not possible here.
        *
        * Therefore DEBUG move output is handled by storing
        * the moves below.
        */

        /*
        * Determine the result from the final board.
        */
        if (game_board->IsCheckMate())
        {
            /*
            * If it is White's turn in checkmate, Black
            * made the winning move.
            */
            const bool white_won =
                game_board->GetTurnColor()
                == TURN_BLACK;

            if (white_won)
            {
                ++white_wins;

                if (notify_game)
                {
                    std::cout
                        << "Result: White wins\n";
                }
            }
            else
            {
                ++black_wins;

                if (notify_game)
                {
                    std::cout
                        << "Result: Black wins\n";
                }
            }
        }
        else
        {
            ++draws;

            if (notify_game)
            {
                if (game_board->IsStaleMate())
                {
                    std::cout
                        << "Result: draw (stalemate)\n";
                }
                else if (game_board->IsThreeFoldRepition())
                {
                    std::cout
                        << "Result: draw "
                        << "(threefold repetition)\n";
                }
                else
                {
                    std::cout
                        << "Result: draw "
                        << "(move limit)\n";
                }
            }
        }

        if (notify_game)
        {
            std::cout
                << "Game finished after "
                << move_count
                << " moves.\n\n";

    #ifdef DEBUG

            PrintBotDebug();
            PrintTTDebug();

            ClearBotDebug();
            ClearTTDebug();

    #endif

            std::cout << "\n\n";
        }
    }

    /*
    * Final statistics.
    */
    const int decisive_games =
        white_wins + black_wins;

    const double white_win_rate =
        white_games > 0
            ? 100.0 * white_wins / NUM_GAMES
            : 0.0;

    const double black_win_rate =
        black_games > 0
            ? 100.0 * black_wins / NUM_GAMES
            : 0.0;

    const double draw_rate =
        pgo_positions.size() > 0
            ? 100.0 * draws / NUM_GAMES
            : 0.0;

    std::cout
        << "========================================\n"
        << "PGO self-play results\n"
        << "========================================\n"
        << "Games:        "
        << NUM_GAMES
        << '\n'
        << "White games:  "
        << white_games
        << '\n'
        << "Black games:  "
        << black_games
        << '\n'
        << "White wins:   "
        << white_wins
        << '\n'
        << "Black wins:   "
        << black_wins
        << '\n'
        << "Draws:        "
        << draws
        << '\n'
        << '\n'
        << "White win %:  "
        << white_win_rate
        << "%\n"
        << "Black win %:  "
        << black_win_rate
        << "%\n"
        << "Draw %:       "
        << draw_rate
        << "%\n";

    if (decisive_games > 0)
    {
        std::cout
            << '\n'
            << "White wins / Black wins: "
            << white_wins
            << " / "
            << black_wins
            << '\n';
    }

    std::cout
        << "========================================\n"
        << "PGO self-play test complete.\n";

#endif


    return 0;
}