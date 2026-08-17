#include "console.hpp"

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
        default:               out = '?'; break;
    }

    if (piece & PIECE_COLOR_WHITE)
        out = char(out - 'a' + 'A');

    return out;
}
}


Console::Console(std::shared_ptr<ChessBoard> _board, std::shared_ptr<ChessBot> _bot) :
    board(_board), bot(_bot)
{
    bot->SetTimeLimit(std::chrono::seconds(1));
}


Console::~Console()
{}


void Console::run()
{
    do
    {
        std::string command = GetCommand();

        std::vector<std::string> args = SplitCommand(command);
        std::string cmd = args[0];

        if (cmd == "move")
        {
            Move move = ParseMove(command);

            if (board->IsLegalMove(move))
                board->MakeMove(move);
            else
                std::cout << "Illegal Move." << std::endl;
            
            // Prints "Check!", "Checkmate!", "Stalemate!", or nothing.
            PrintEndgame();
        }

        else if (cmd == "fen")
        {
            ParseFEN(command);
        }

        else if (cmd == "reset")
        {
            board->LoadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }

        else if (cmd == "board")
        {
            PrintBoard();
        }

        else if (cmd == "bot")
        {
            const int min_depth = 3;
            const int max_depth = 100;
            MoveResult move = bot->Search(min_depth, max_depth);
            std::cout << "[BOT] Has searched to a depth of " << move.depth
                << " and has searched " << move.nodes_searched << " nodes."
                << std::endl;

            std::cout << "[BOT] Has decided to play " << MoveToString(move.move) << std::endl;
            std::cout << "[TT] Transposition table has " << bot->GetTranspositionTableSize() << " entries." << std::endl;
            board->MakeMove(move.move);

            // Prints "Check!", "Checkmate!", "Stalemate!", or nothing.
            PrintEndgame();
            continue;
        }

        else if (cmd == "eval")
        {
            std::cout << "Current position evaluation (quiesence search): " << bot->Evaluate() << "." << std::endl;
            std::cout << "Current position evaluation (raw): " << bot->EvaluateRaw() << "." << std::endl;
        }

        else if (cmd == "train")
        {
            DurationMs original_timelimit = bot->GetTimeLimit();

            std::string timelimit_str = GetCommand("How long would you like to train the bot (seconds)? ");
            double timelimit = std::stod(timelimit_str);
            int timelimit_ms = (int)(timelimit * 1000);

            std::cout << "Training..." << std::flush;

            // Training time limit.
            bot->SetTimeLimit(DurationMs(timelimit_ms));

            // Train on the starting positon to warm up the transposition table.
            bot->Search(5, 100);

            std::cout << "Done." << std::endl;
            std::cout << "[TT] Transposition table has " << bot->GetTranspositionTableSize() << " entries." << std::endl;

            // Reset to the original time limit
            bot->SetTimeLimit(original_timelimit);
        }

        else if (cmd == "timelimit")
        {
            std::string timelimit_str = GetCommand("How long would you like the bot to think (seconds)? ");
            double timelimit = std::stod(timelimit_str);
            int timelimit_ms = (int)(timelimit * 1000);

            std::cout << "Set [BOT] time limit to " << timelimit_ms << "ms." << std::endl;
            bot->SetTimeLimit(DurationMs(timelimit_ms));
        }

        else if (cmd == "exit")
        {
            break;
        }

        else
        {
            std::cout << "Invalid command: `" << cmd << "`" << std::endl;
        }

    } while (1);
}


void Console::PrintEndgame()
{
    if (board->IsCheckMate())
    {
        // Checkmate outranks check.
        std::cout << "Checkmate!" << std::endl;
        return;
    }

    if (board->IsStaleMate())
    {
        // Checkmate outranks check.
        std::cout << "Stalemate!" << std::endl;
        return;
    }

    if (board->IsCheck())
    {
        std::cout << "Check!" << std::endl;
    }
}


std::string Console::GetCommand(const std::string& prompt)
{
    std::string buffer;
    std::cout << prompt;
    std::getline(std::cin, buffer);

    // If the first getline consumed a leftover newline, read again
    if (buffer.empty() && !std::cin.eof())
        std::getline(std::cin, buffer);

    return buffer;
}


std::vector<std::string> Console::SplitCommand(std::string str)
{
    std::istringstream stream(str);
    std::string part;
    std::vector<std::string> parts;

    while (stream >> part)
    {
        parts.push_back(part);
    }

    return parts;
}


Move Console::ParseMove(std::string move_str)
{
    Move move;
    std::size_t p = 4;

    // Skip spaces
    while (move_str[p] == ' ')
        p++;

    // From
    uint8_t fromx = 0, fromy = 0;
    if ((move_str[p] >= 'a') && (move_str[p] <= 'h'))
    {
        fromx = move_str[p] - 'a';
        p++;
    }
    if ((move_str[p] >= '1') && (move_str[p] <= '8'))
    {
        fromy = move_str[p] - '1';
        p++;
    }

    move.from = flatten_xy(fromx, fromy);

    // Skip spaces
    while (move_str[p] == ' ')
        p++;

    // To
    uint8_t tox = 0, toy = 0;
    if ((move_str[p] >= 'a') && (move_str[p] <= 'h'))
    {
        tox = move_str[p] - 'a';
        p++;
    }
    if ((move_str[p] >= '1') && (move_str[p] <= '8'))
    {
        toy = move_str[p] - '1';
        p++;
    }

    move.to = flatten_xy(tox, toy);

    return move;
}


void Console::ParseFEN(std::string fen_str)
{
    std::size_t p = fen_str.find_first_of(' ');
    if (p == std::string::npos)
        return;

    std::size_t start = p + 1;
    while (start < fen_str.size() && fen_str[start] == ' ')
        ++start;

    if (start >= fen_str.size())
        return;

    std::string fen = fen_str.substr(start);
    board->LoadFEN(fen);
}


std::string Console::MoveToString(Move move)
{
    char buff[5] = {0,0,0,0,0};

    buff[0] = get_piece_x(move.from) + 'a';
    buff[1] = get_piece_y(move.from) + '1';
    buff[2] = get_piece_x(move.to) + 'a';
    buff[3] = get_piece_y(move.to) + '1';

    return std::string(buff);
}

void Console::PrintBoard()
{
    std::cout << (board->GetTurnColor() == TURN_WHITE ? "White to move" : "Black to move") << std::endl;
    std::cout << "  | a  b  c  d  e  f  g  h |" << std::endl;
    std::cout << "  +------------------------+" << std::endl;

    for (int8_t y = 7; y >= 0; y--)
    {
        printf("%c |", '1' + y);

        for (uint8_t x = 0; x < 8; x++)
        {
            uint8_t coord = flatten_xy(x, y);
            Piece piece = board->GetPieceAt(coord);
            printf(" %c ", piece_to_char(piece));
        }

        printf("| %c\n", '1' + y);
    }

    std::cout << "  +------------------------+" << std::endl;
    std::cout << "  | a  b  c  d  e  f  g  h |" << std::endl;
}

