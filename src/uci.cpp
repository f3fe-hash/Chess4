#include "uci.hpp"


std::string UCI::Respond(const std::string& request)
{
    if (
        (request == "quit") || // Quit
        (request == "stop") || // Stop calculation as soon as possible
        (request == "setoption") || // Set an option
        (request == "ponderhit") // The user has played the expected move.
    )
        return "";
    
    if (request == "uci")
        return "uciok";
    
    if (request == "isready")
        return "readyok";
    
    if (request == "ucinewgame")
    {
        // New game
        board.reset();

        board = std::make_shared<ChessBoard>();
    }


    return "";
}


Move UCI::StringToMove(std::string str)
{
    Move move;

    move.from = flatten_xy(
        str[0] - 'a',
        str[1] - '1'
    );

    move.to = flatten_xy(
        str[2] - 'a',
        str[3] - '1'
    );

    return move;
}


std::string UCI::MoveToString(const Move& move)
{
    std::string str;
    str.reserve(4);

    str.push_back(char(get_piece_x(move.from) + 'a'));
    str.push_back(char(get_piece_y(move.from) + '1'));
    str.push_back(char(get_piece_x(move.to)   + 'a'));
    str.push_back(char(get_piece_y(move.to)   + '1'));

    return str;
}



