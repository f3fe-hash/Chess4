#include "interface/uci.hpp"


namespace
{

constexpr const char* STARTPOS_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/"
    "PPPPPPPP/RNBQKBNR w KQkq - 0 1";


char PromotionToChar(Piece piece)
{
    switch (piece & 0x07)
    {
        case PIECE_TYPE_KNIGHT:
            return 'n';

        case PIECE_TYPE_BISHOP:
            return 'b';

        case PIECE_TYPE_ROOK:
            return 'r';

        case PIECE_TYPE_QUEEN:
            return 'q';

        default:
            return '\0';
    }
}


Piece CharToPromotion(char c, TurnColor color)
{
    uint8_t color_bits =
        color ? PIECE_COLOR_WHITE : PIECE_COLOR_BLACK;

    switch (std::tolower(
        static_cast<unsigned char>(c)))
    {
        case 'n':
            return PIECE_TYPE_KNIGHT | color_bits;

        case 'b':
            return PIECE_TYPE_BISHOP | color_bits;

        case 'r':
            return PIECE_TYPE_ROOK | color_bits;

        case 'q':
            return PIECE_TYPE_QUEEN | color_bits;

        default:
            return NULL_PIECE;
    }
}

} // namespace


UCI::UCI(
    std::shared_ptr<ChessBoard> _board,
    std::shared_ptr<ChessBot> _bot
) :
    board(std::move(_board)),
    bot(std::move(_bot))
{
    if (!board)
        board = std::make_shared<ChessBoard>();

    if (!bot)
        bot = std::make_shared<ChessBot>(board);
}


UCI::~UCI()
{
    StopSearch();
}


std::vector<std::string> UCI::Tokenize(
    const std::string& string)
{
    std::vector<std::string> tokens;

    std::istringstream stream(string);

    std::string token;

    while (stream >> token)
        tokens.push_back(token);

    return tokens;
}


bool UCI::IsInteger(const std::string& string)
{
    if (string.empty())
        return false;

    size_t start = 0;

    if (string[0] == '-' || string[0] == '+')
        start = 1;

    if (start == string.size())
        return false;

    for (size_t i = start; i < string.size(); ++i)
    {
        if (!std::isdigit(
            static_cast<unsigned char>(string[i])))
        {
            return false;
        }
    }

    return true;
}


int UCI::ParseInteger(
    const std::string& string,
    int default_value)
{
    if (!IsInteger(string))
        return default_value;

    try
    {
        return std::stoi(string);
    }
    catch (...)
    {
        return default_value;
    }
}


std::string UCI::HandleUCI()
{
    std::string out;

    out += "id name FastKat\n";
    out += "id author Brian\n";

    // These are harmless defaults for now. If you later add
    // configurable options to ChessBot, handle them here.
    out += "option name Hash type spin default 64 min 1 max 4096\n";

    out += "uciok\n";

    return out;
}


std::string UCI::HandleReady()
{
    return "readyok\n";
}


std::string UCI::HandleNewGame()
{
    StopSearch();

    std::lock_guard<std::mutex> lock(board_mutex);

    board->LoadFEN(STARTPOS_FEN);

    return "";
}


std::string UCI::MoveToString(const Move& move) const
{
    // Null move
    if ((move.to == 0) && (move.from == 0))
        return "0000";
    
    
    
    std::string result;

    result.reserve(5);

    result.push_back(
        static_cast<char>(
            get_piece_x(move.from) + 'a'));

    result.push_back(
        static_cast<char>(
            get_piece_y(move.from) + '1'));

    result.push_back(
        static_cast<char>(
            get_piece_x(move.to) + 'a'));

    result.push_back(
        static_cast<char>(
            get_piece_y(move.to) + '1'));

    if (move.promotion != NULL_PIECE)
    {
        char promotion =
            PromotionToChar(move.promotion);

        if (promotion != '\0')
            result.push_back(promotion);
    }

    return result;
}


Move UCI::StringToMove(const std::string& string)
{
    Move move{};

    move.from = flatten_xy(
        string[0] - 'a',
        string[1] - '1');

    move.to = flatten_xy(
        string[2] - 'a',
        string[3] - '1');

    move.moved =
        board->GetPieceAt(move.from);

    move.captured =
        board->GetPieceAt(move.to);

    move.promotion = NULL_PIECE;

    if (string.size() >= 5)
    {
        move.promotion =
            CharToPromotion(
                string[4],
                board->GetTurnColor());
    }

    move.flags = MOVE_NORMAL;

    return move;
}


bool UCI::FindLegalMove(
    const std::string& string,
    Move& result)
{
    if (string.size() != 4 &&
        string.size() != 5)
    {
        return false;
    }

    if (string[0] < 'a' || string[0] > 'h' ||
        string[2] < 'a' || string[2] > 'h' ||
        string[1] < '1' || string[1] > '8' ||
        string[3] < '1' || string[3] > '8')
    {
        return false;
    }

    const int from = flatten_xy(
        string[0] - 'a',
        string[1] - '1');

    const int to = flatten_xy(
        string[2] - 'a',
        string[3] - '1');

    Piece promotion = NULL_PIECE;

    if (string.size() == 5)
    {
        promotion = CharToPromotion(
            string[4],
            board->GetTurnColor());

        if (promotion == NULL_PIECE)
            return false;
    }

    const std::vector<Move> legal_moves =
        board->GetLegalMoves();

    for (const Move& legal : legal_moves)
    {
        if (legal.from != from ||
            legal.to != to)
        {
            continue;
        }

        if (legal.promotion != promotion)
            continue;

        result = legal;
        return true;
    }

    return false;
}


bool UCI::ApplyUCIMove(
    const std::string& move_string)
{
    Move move;

    if (!FindLegalMove(move_string, move))
        return false;

    board->MakeMove(move);

    return true;
}


bool UCI::SetPosition(
    const std::string& command)
{
    std::vector<std::string> tokens =
        Tokenize(command);

    if (tokens.size() < 2)
        return false;

    size_t index = 1;

    {
        std::lock_guard<std::mutex> lock(board_mutex);

        if (tokens[index] == "startpos")
        {
            if (!board->LoadFEN(STARTPOS_FEN))
                return false;

            ++index;
        }
        else if (tokens[index] == "fen")
        {
            ++index;

            std::string fen;

            // FEN consists of six fields. Collect them until
            // "moves" or six fields have been consumed.
            int fields = 0;

            while (index < tokens.size() &&
                   tokens[index] != "moves" &&
                   fields < 6)
            {
                if (!fen.empty())
                    fen += ' ';

                fen += tokens[index];

                ++index;
                ++fields;
            }

            if (fields != 6)
                return false;

            if (!board->LoadFEN(fen))
                return false;
        }
        else
        {
            return false;
        }

        if (index < tokens.size() &&
            tokens[index] == "moves")
        {
            ++index;

            while (index < tokens.size())
            {
                if (!ApplyUCIMove(tokens[index]))
                    return false;

                ++index;
            }
        }
    }

    return true;
}


std::string UCI::HandlePosition(
    const std::string& command)
{
    StopSearch();

    if (!SetPosition(command))
        return "info string invalid position\n";

    return "";
}


std::string UCI::HandleGo(
    const std::string& command)
{
    if (searching.load())
        return "";

    std::vector<std::string> tokens =
        Tokenize(command);

    // Reset search parameters.
    search_depth = 100;
    search_node_limit = 0;

    search_time = DurationMs(0);

    white_time = DurationMs(0);
    black_time = DurationMs(0);

    white_increment = DurationMs(0);
    black_increment = DurationMs(0);

    infinite_search = false;

    bool depth_set = false;
    bool movetime_set = false;
    bool nodes_set = false;
    bool ponder_search = false;

    for (size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string& token = tokens[i];

        if (token == "depth" && i + 1 < tokens.size())
        {
            search_depth =
                std::max(
                    1,
                    ParseInteger(
                        tokens[++i],
                        100));

            depth_set = true;
        }
        else if (token == "movetime" &&
                 i + 1 < tokens.size())
        {
            search_time =
                DurationMs(
                    std::max(
                        1,
                        ParseInteger(
                            tokens[++i],
                            1)));

            movetime_set = true;
        }
        else if (token == "nodes" &&
                 i + 1 < tokens.size())
        {
            try
            {
                search_node_limit =
                    std::stoull(tokens[++i]);

                nodes_set = true;
            }
            catch (...)
            {
                search_node_limit = 0;
            }
        }
        else if (token == "wtime" &&
                 i + 1 < tokens.size())
        {
            white_time =
                DurationMs(
                    std::max(
                        0,
                        ParseInteger(
                            tokens[++i],
                            0)));

            if (original_white_time.count() == 0)
                original_white_time = white_time;
        }
        else if (token == "btime" &&
                 i + 1 < tokens.size())
        {
            black_time =
                DurationMs(
                    std::max(
                        0,
                        ParseInteger(
                            tokens[++i],
                            0)));

            if (original_black_time.count() == 0)
                original_black_time = black_time;
        }
        else if (token == "winc" &&
                 i + 1 < tokens.size())
        {
            white_increment =
                DurationMs(
                    std::max(
                        0,
                        ParseInteger(
                            tokens[++i],
                            0)));
        }
        else if (token == "binc" &&
                 i + 1 < tokens.size())
        {
            black_increment =
                DurationMs(
                    std::max(
                        0,
                        ParseInteger(
                            tokens[++i],
                            0)));
        }
        else if (token == "infinite")
        {
            infinite_search = true;
        }
        else if (token == "ponder")
        {
            ponder_search = true;
        }
    }

    /*
     * Determine time from the clock if movetime wasn't
     * explicitly specified.
     */
    if (!movetime_set &&
        !depth_set &&
        !nodes_set &&
        !infinite_search)
    {
        search_time =
            bot->CalculateThinkTime(
                white_time,
                black_time,
                original_white_time,
                original_black_time,
                white_increment,
                black_increment);
    }

    if (infinite_search)
    {
        /*
         * The current ChessBot interface does not have an
         * infinite-search API. Use the maximum duration and
         * rely on the search's stop mechanism.
         */
        search_time =
            DurationMs::max();
    }

    ponder_search_time = search_time;
    if (ponder_search)
        search_time = DurationMs::max();

    pondering.store(ponder_search);
    StartSearch();
    return "";
}


void UCI::StartSearch()
{
    StopSearch();

    searching.store(true);

    search_thread =
        std::thread(
            &UCI::SearchThread,
            this);
}


void UCI::StopSearch()
{
    if (!searching.load())
    {
        if (search_thread.joinable())
            search_thread.join();

        return;
    }

    bot->Stop();

    if (search_thread.joinable())
        search_thread.join();

    searching.store(false);
}


void UCI::SearchThread()
{
    {
        std::lock_guard<std::mutex> lock(board_mutex);

        bot->SetTimeLimit(search_time);
    }

    MoveResult result;

    {
        std::lock_guard<std::mutex> lock(board_mutex);

        result =
            bot->Search(
                1,
                search_depth);
    }

    {
        std::lock_guard<std::mutex> lock(board_mutex);

        /*
         * UCI requires bestmove even if the search was stopped.
         */
        std::string output =
            "bestmove " +
            MoveToString(result.move) +
            "\n";

        {
            std::lock_guard<std::mutex> lock(output_mutex);
            pending_output += output;
        }
    }

    pondering.store(false);
    searching.store(false);
}


std::string UCI::HandleStop()
{
    StopSearch();

    return TakeOutput();
}


std::string UCI::HandleQuit()
{
    StopSearch();

    quit_requested.store(true);

    return TakeOutput();
}


std::string UCI::HandleSetOption(
    const std::string& command)
{
    (void) command;
    return "";
}


std::string UCI::HandlePonderHit()
{
    if (!pondering.load())
        return "info string ponderhit without ponder search\n";

    pondering.store(false);
    bot->SetTimeLimit(ponder_search_time);
    return "";
}


std::string UCI::TakeOutput()
{
    std::lock_guard<std::mutex> lock(output_mutex);
    std::string output;
    output.swap(pending_output);
    return output;
}


std::string UCI::Respond(
    const std::string& request)
{
    std::vector<std::string> tokens =
        Tokenize(request);

    if (tokens.empty())
        return "";

    const std::string& command =
        tokens[0];

    if (command == "uci")
        return HandleUCI();

    else if (command == "isready")
        return HandleReady();

    else if (command == "ucinewgame")
        return HandleNewGame();

    else if (command == "position")
        return HandlePosition(request);

    else if (command == "go")
        return HandleGo(request);

    else if (command == "stop")
        return HandleStop();

    else if (command == "quit")
        return HandleQuit();

    else if (command == "setoption")
        return HandleSetOption(request);

    else if (command == "ponderhit")
        return HandlePonderHit();

    // UCI GUIs sometimes send debug commands.
    if (command == "debug")
        return "idk this isnt implemented";

    return "info string unknown command " + command + "\n";
}

