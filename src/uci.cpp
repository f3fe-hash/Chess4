#include "uci.hpp"


std::vector<uint8_t> UCIInterface::HandleInit()
{}


std::vector<uint8_t> UCIInterface::HandleBestmove(std::vector<Move> moves)
{
    std::vector<uint8_t> str;
    str.push_back("bestmove ");
    
    Move bestmove = bot->Search(3, 100);
    std::vector<uint8_t> bestmove_str = MoveToString(bestmove);

    InsertVector(str, bestmove);
    return str;
}


Move UCIInterface::StringToMove(std::vector<uint8_t> str)
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


std::vector<uint8_t> UCIInterface::MoveToString(const Move& move)
{
    std::vector<uint8_t> str;

    str.push_back(get_piece_x(move.from) + 'a');
    str.push_back(get_piece_y(move.from) + '1');
    str.push_back(get_piece_x(move.to) + 'a');
    str.push_back(get_piece_y(move.to) + '1');
    str.push_back(0);

    return str;
}


