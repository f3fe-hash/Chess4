#include "console.hpp"

#include <ncurses.h>

namespace
{

char piece_to_char(Piece piece)
{
    if (piece == NULL_PIECE)
        return '.';

    char out = '?';

    switch (piece & 0x07)
    {
        case PIECE_TYPE_PAWN:   out = 'p'; break;
        case PIECE_TYPE_KNIGHT: out = 'n'; break;
        case PIECE_TYPE_BISHOP: out = 'b'; break;
        case PIECE_TYPE_ROOK:   out = 'r'; break;
        case PIECE_TYPE_QUEEN:  out = 'q'; break;
        case PIECE_TYPE_KING:   out = 'k'; break;
        default:                out = '?'; break;
    }

    if (piece & PIECE_COLOR_WHITE)
        out = static_cast<char>(out - 'a' + 'A');

    return out;
}

} // namespace


Console::Console(
    std::shared_ptr<ChessBoard> _board,
    std::shared_ptr<ChessBot> _bot
) :
    board(_board),
    bot(_bot)
{
    bot->SetTimeLimit(std::chrono::seconds(1));
}


Console::~Console()
{
}


void Console::run()
{
    initscr();

    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    if (has_colors())
    {
        start_color();

        // Board colors.
        init_pair(1, COLOR_BLACK, COLOR_WHITE); // Light square
        init_pair(2, COLOR_WHITE, COLOR_BLACK); // Dark square

        // UI colors.
        init_pair(3, COLOR_CYAN, COLOR_BLACK);  // Headers
        init_pair(4, COLOR_YELLOW, COLOR_BLACK); // Status
        init_pair(5, COLOR_RED, COLOR_BLACK);    // Errors
        init_pair(6, COLOR_GREEN, COLOR_BLACK);  // Input
    }

    // Don't show the cursor until we're actually entering text.
    curs_set(0);

    std::string message;

    while (true)
    {
        clear();

        PrintBoard(message);

        refresh();

        std::string command = GetCommand();

        std::vector<std::string> args = SplitCommand(command);

        if (args.empty())
            continue;

        const std::string& cmd = args[0];

        message.clear();

        // ------------------------------------------------------------
        // move
        // ------------------------------------------------------------

        if (cmd == "move")
        {
            Move move = ParseMove(command);

            if (board->IsLegalMove(move))
            {
                board->MakeMove(move);
                PrintEndgame(message);
            }
            else
            {
                message = "Illegal move.";
            }
        }

        // ------------------------------------------------------------
        // fen
        // ------------------------------------------------------------

        else if (cmd == "fen")
        {
            ParseFEN(command);
            message = "FEN loaded.";
        }

        // ------------------------------------------------------------
        // reset
        // ------------------------------------------------------------

        else if (cmd == "reset")
        {
            board->LoadFEN(
                "rnbqkbnr/pppppppp/8/8/8/8/"
                "PPPPPPPP/RNBQKBNR w KQkq - 0 1"
            );

            message = "Board reset.";
        }

        // ------------------------------------------------------------
        // board
        // ------------------------------------------------------------

        else if (cmd == "board")
        {
            // The board is always rendered.
        }

        // ------------------------------------------------------------
        // bot
        // ------------------------------------------------------------

        else if (cmd == "bot")
        {
            message = "Bot thinking...";

            clear();
            PrintBoard(message);
            refresh();

            const int min_depth = 3;
            const int max_depth = 100;

            MoveResult move = bot->Search(min_depth, max_depth);

            board->MakeMove(move.move);

            message =
                "BOT: depth " + std::to_string(move.depth) +
                " | nodes " + std::to_string(move.nodes_searched) +
                " | move " + MoveToString(move.move) +
                " | TT " + std::to_string(
                    bot->GetTranspositionTableSize()
                );

            PrintEndgame(message);
        }

        // ------------------------------------------------------------
        // eval
        // ------------------------------------------------------------

        else if (cmd == "eval")
        {
            const double eval = bot->Evaluate();
            const double raw = bot->EvaluateRaw();

            message =
                "Evaluation: " + std::to_string(eval) +
                " | Raw: " + std::to_string(raw);
        }

        // ------------------------------------------------------------
        // train
        // ------------------------------------------------------------

        else if (cmd == "train")
        {
            DurationMs original_timelimit = bot->GetTimeLimit();

            const std::string timelimit_str =
                GetCommand("Train time (seconds): ");

            try
            {
                const double timelimit = std::stod(timelimit_str);

                if (timelimit <= 0.0)
                {
                    message = "Training time must be greater than zero.";
                    continue;
                }

                const int timelimit_ms =
                    static_cast<int>(timelimit * 1000.0);

                bot->SetTimeLimit(DurationMs(timelimit_ms));

                clear();
                PrintBoard("Training...");
                refresh();

                // Warm up the transposition table.
                bot->Search(5, 100);

                message =
                    "Training complete | TT: " +
                    std::to_string(
                        bot->GetTranspositionTableSize()
                    ) +
                    " entries.";

                bot->SetTimeLimit(original_timelimit);
            }
            catch (const std::exception&)
            {
                bot->SetTimeLimit(original_timelimit);
                message = "Invalid training time.";
            }
        }

        // ------------------------------------------------------------
        // timelimit
        // ------------------------------------------------------------

        else if (cmd == "timelimit")
        {
            const std::string timelimit_str =
                GetCommand("Bot time limit (seconds): ");

            try
            {
                const double timelimit = std::stod(timelimit_str);

                if (timelimit <= 0.0)
                {
                    message = "Time limit must be greater than zero.";
                    continue;
                }

                const int timelimit_ms =
                    static_cast<int>(timelimit * 1000.0);

                bot->SetTimeLimit(DurationMs(timelimit_ms));

                message =
                    "Bot time limit set to " +
                    std::to_string(timelimit_ms) +
                    "ms.";
            }
            catch (const std::exception&)
            {
                message = "Invalid time limit.";
            }
        }

        // ------------------------------------------------------------
        // exit
        // ------------------------------------------------------------

        else if (cmd == "exit" || cmd == "quit")
        {
            break;
        }

        // ------------------------------------------------------------
        // invalid
        // ------------------------------------------------------------

        else
        {
            message = "Invalid command: " + cmd;
        }
    }

    endwin();
}


void Console::PrintEndgame(std::string& message)
{
    if (board->IsCheckMate())
    {
        message = "Checkmate!";
        return;
    }

    if (board->IsStaleMate())
    {
        message = "Stalemate!";
        return;
    }

    if (board->IsCheck())
    {
        message = "Check!";
    }
}


std::string Console::GetCommand(const std::string& prompt)
{
    const int input_y = 16;

    // Keep the cursor inside the input box.
    curs_set(1);

    if (has_colors())
        attron(COLOR_PAIR(6));

    mvprintw(input_y, 4, "> ");

    if (!prompt.empty())
    {
        mvprintw(input_y - 1, 4, "%s", prompt.c_str());
    }

    if (has_colors())
        attroff(COLOR_PAIR(6));

    refresh();

    char input[256] = {};

    echo();
    getnstr(input, sizeof(input) - 1);
    noecho();

    curs_set(0);

    return std::string(input);
}


std::vector<std::string> Console::SplitCommand(std::string str)
{
    std::istringstream stream(str);

    std::string part;
    std::vector<std::string> parts;

    while (stream >> part)
        parts.push_back(part);

    return parts;
}


Move Console::ParseMove(std::string move_str)
{
    Move move{};

    const std::vector<std::string> args = SplitCommand(move_str);

    if (args.size() < 2)
        return move;

    const std::string& notation = args[1];

    // We require exactly at least four characters:
    //
    // e2e4
    //
    if (notation.size() < 4)
        return move;

    const char from_file = notation[0];
    const char from_rank = notation[1];
    const char to_file   = notation[2];
    const char to_rank   = notation[3];

    // Validate coordinates before converting them.
    if (from_file < 'a' || from_file > 'h' ||
        to_file   < 'a' || to_file   > 'h' ||
        from_rank < '1' || from_rank > '8' ||
        to_rank   < '1' || to_rank   > '8')
    {
        return move;
    }

    const uint8_t fromx =
        static_cast<uint8_t>(from_file - 'a');

    const uint8_t fromy =
        static_cast<uint8_t>(from_rank - '1');

    const uint8_t tox =
        static_cast<uint8_t>(to_file - 'a');

    const uint8_t toy =
        static_cast<uint8_t>(to_rank - '1');

    move.from = flatten_xy(fromx, fromy);
    move.to = flatten_xy(tox, toy);

    return move;
}


void Console::ParseFEN(std::string fen_str)
{
    const std::size_t p = fen_str.find_first_of(' ');

    if (p == std::string::npos)
        return;

    std::size_t start = p + 1;

    while (start < fen_str.size() && fen_str[start] == ' ')
        ++start;

    if (start >= fen_str.size())
        return;

    const std::string fen = fen_str.substr(start);

    board->LoadFEN(fen);
}


std::string Console::MoveToString(Move move)
{
    char buff[5] = {0, 0, 0, 0, 0};

    buff[0] = get_piece_x(move.from) + 'a';
    buff[1] = get_piece_y(move.from) + '1';
    buff[2] = get_piece_x(move.to) + 'a';
    buff[3] = get_piece_y(move.to) + '1';

    return std::string(buff);
}


void Console::PrintBoard(const std::string& message)
{
    // ------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------

    constexpr int board_x = 2;
    constexpr int board_y = 1;

    constexpr int square_width = 3;
    constexpr int board_squares = 8;

    // 8 squares * 3 chars = 24
    // + 1 column for each rank label = 26
    constexpr int board_width =
        5 + board_squares * square_width;

    // Top labels + 8 board rows + bottom labels
    constexpr int board_height =
        board_squares + 4;

    // Coordinates inside the board.
    //constexpr int board_start_x = board_x + 1;
    //constexpr int board_start_y = board_y + 1;

    // Input box.
    constexpr int input_x = 2;
    constexpr int input_y = 14;
    constexpr int input_width = 60;
    constexpr int input_height = 4;

    // Make sure the terminal is large enough.
    if (LINES < input_y + input_height + 1 ||
        COLS < input_x + input_width + 1)
    {
        mvprintw(
            0,
            0,
            "Terminal too small. Please resize."
        );

        return;
    }

    // ------------------------------------------------------------
    // Board border
    // ------------------------------------------------------------

    if (has_colors())
        attron(COLOR_PAIR(3));

    const int right_border_x =
        board_x + board_width;

    const int bottom_border_y =
        board_y + board_height;

    mvaddch(
        board_y,
        board_x,
        ACS_ULCORNER
    );

    mvaddch(
        board_y,
        right_border_x,
        ACS_URCORNER
    );

    mvaddch(
        bottom_border_y,
        board_x,
        ACS_LLCORNER
    );

    mvaddch(
        bottom_border_y,
        right_border_x,
        ACS_LRCORNER
    );

    mvhline(
        board_y,
        board_x + 1,
        ACS_HLINE,
        board_width - 1
    );

    mvhline(
        bottom_border_y,
        board_x + 1,
        ACS_HLINE,
        board_width - 1
    );

    mvvline(
        board_y + 1,
        board_x,
        ACS_VLINE,
        board_height - 1
    );

    mvvline(
        board_y + 1,
        right_border_x,
        ACS_VLINE,
        board_height - 1
    );

    if (has_colors())
        attroff(COLOR_PAIR(3));

    // ------------------------------------------------------------
    // Turn indicator
    // ------------------------------------------------------------

    const char* turn =
        board->GetTurnColor() == TURN_WHITE
            ? "White to move"
            : "Black to move";

    if (has_colors())
        attron(COLOR_PAIR(3) | A_BOLD);

    mvprintw(
        board_y + 1,
        board_x + 1,
        " %s",
        turn
    );

    if (has_colors())
        attroff(COLOR_PAIR(3) | A_BOLD);

    // ------------------------------------------------------------
    // Column labels
    // ------------------------------------------------------------

    // The first square starts at board_x + 2.
    //
    // Since each square is three characters wide:
    //
    //   a      b      c
    //  ---    ---    ---
    //
    // Put the label in the center of each square.
    //
    const int squares_x = board_x + 2;

    if (has_colors())
        attron(COLOR_PAIR(3));

    for (int x = 0; x < board_squares; ++x)
    {
        const int center_x =
            squares_x + x * square_width + 1;

        mvaddch(
            board_y + 2,
            center_x + 1,
            static_cast<char>('a' + x)
        );
    }

    if (has_colors())
        attroff(COLOR_PAIR(3));

    // ------------------------------------------------------------
    // Chessboard
    // ------------------------------------------------------------

    for (int y = 7; y >= 0; --y)
    {
        // Board rows start after the top labels.
        const int row =
            board_y + 3 + (7 - y);

        // --------------------------------------------------------
        // Rank on the left
        // --------------------------------------------------------

        if (has_colors())
            attron(COLOR_PAIR(3));

        mvaddch(
            row,
            board_x + 1,
            static_cast<char>('1' + y)
        );

        mvaddch(
            row,
            board_x + 2,
            ' '
        );

        if (has_colors())
            attroff(COLOR_PAIR(3));

        // --------------------------------------------------------
        // Squares
        // --------------------------------------------------------

        for (uint8_t x = 0; x < 8; ++x)
        {
            const uint8_t coord =
                flatten_xy(x, y);

            const Piece piece =
                board->GetPieceAt(coord);

            const char piece_char =
                piece_to_char(piece);

            const int screen_x =
                squares_x + x * square_width + 1;

            const bool light_square =
                ((x + y) % 2) == 1;

            if (has_colors())
            {
                const int color_pair =
                    light_square ? 1 : 2;

                attron(COLOR_PAIR(color_pair));

                // If it is white, make it bold.
                if ((piece_char >= 'A') && (piece_char <= 'Z'))
                    attron(A_BOLD);

                mvprintw(
                    row,
                    screen_x,
                    " %c ",
                    piece_char
                );

                // If it is white, it is bold.
                if ((piece_char >= 'A') && (piece_char <= 'Z'))
                    attroff(A_BOLD);

                attroff(COLOR_PAIR(color_pair));
            }
            else
            {
                mvprintw(
                    row,
                    screen_x,
                    " %c ",
                    piece_char
                );
            }
        }

        // --------------------------------------------------------
        // Rank on the right
        // --------------------------------------------------------

        if (has_colors())
            attron(COLOR_PAIR(3));

        mvaddch(
            row,
            squares_x + board_squares * square_width + 2,
            static_cast<char>('1' + y)
        );

        if (has_colors())
            attroff(COLOR_PAIR(3));
    }

    // ------------------------------------------------------------
    // Bottom column labels
    // ------------------------------------------------------------

    const int bottom_labels_y =
        board_y + 11;

    if (has_colors())
        attron(COLOR_PAIR(3));

    for (int x = 0; x < board_squares; ++x)
    {
        const int center_x =
            squares_x + x * square_width + 1;

        mvaddch(
            bottom_labels_y,
            center_x + 1,
            static_cast<char>('a' + x)
        );
    }

    if (has_colors())
        attroff(COLOR_PAIR(3));

    // ------------------------------------------------------------
    // Input box
    // ------------------------------------------------------------

    if (has_colors())
        attron(COLOR_PAIR(3));

    mvaddch(
        input_y,
        input_x,
        ACS_ULCORNER
    );

    mvaddch(
        input_y,
        input_x + input_width,
        ACS_URCORNER
    );

    mvaddch(
        input_y + input_height,
        input_x,
        ACS_LLCORNER
    );

    mvaddch(
        input_y + input_height,
        input_x + input_width,
        ACS_LRCORNER
    );

    mvhline(
        input_y,
        input_x + 1,
        ACS_HLINE,
        input_width - 1
    );

    mvhline(
        input_y + input_height,
        input_x + 1,
        ACS_HLINE,
        input_width - 1
    );

    mvvline(
        input_y + 1,
        input_x,
        ACS_VLINE,
        input_height - 1
    );

    mvvline(
        input_y + 1,
        input_x + input_width,
        ACS_VLINE,
        input_height - 1
    );

    if (has_colors())
        attroff(COLOR_PAIR(3));

    // ------------------------------------------------------------
    // Status
    // ------------------------------------------------------------

    if (!message.empty())
    {
        if (has_colors())
            attron(COLOR_PAIR(4));

        mvprintw(
            input_y + 2,
            input_x + 5,
            "%-*s",
            input_width - 3,
            message.c_str()
        );

        if (has_colors())
            attroff(COLOR_PAIR(4));
    }
}
