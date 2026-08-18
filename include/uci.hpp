#pragma once

#include <string>

#include "chess.hpp"
#include "core/bot.hpp"

class UCI
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<ChessBot> bot;

    std::string HandleRawBestmove(std::string cmd);
    std::string HandleBestmove(std::vector<Move> moves);

    Move StringToMove(std::string str);
    std::string MoveToString(const Move& move);

    inline void AppendString(std::string& a, const std::string& b)
    {
        a.reserve(a.size() + b.size());
        a.insert(a.end(), b.begin(), b.end());
    }

public:
    UCI() {}
    ~UCI() {}

    std::string Respond(const std::string& request);
};


