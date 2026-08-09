#include "console.hpp"


Console::Console(std::shared_ptr<ChessBoard> _board, std::shared_ptr<ChessBot> _bot) :
    board(_board), bot(_bot)
{}


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
            
            if (board->IsCheckMate())
            {
                // Checkmate outranks check.
                std::cout << "Checkmate!" << std::endl;
                continue;
            }

            if (board->IsCheck())
            {
                std::cout << "Check!" << std::endl;
                continue;
            }
        }
        else if (cmd == "fen")
        {
            ParseFEN(command);
        }
        else if (cmd == "reset")
        {
            board->LoadFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }
        else if (cmd == "bot")
        {
            MoveResult move = bot->Search(5);
            std::cout << "[BOT] Has decided to play " << MoveToString(move.move) << std::endl;
            continue;
        }
        else if (cmd == "eval")
        {
            // TODO: Evaluate position.
            continue;
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


std::string Console::GetCommand()
{
    std::string buffer;
    std::cout << ">>> ";
    std::cin >> buffer;

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
    std::size_t p = 3;

    // Skip spaces
    while (fen_str[p] == ' ')
        p++;
    
    std::string fen = fen_str.substr(p);

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

