#pragma once

#include "chess.hpp"
#include "bot.hpp"

class UCIInterface
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<ChessBot> bot;

    std::vector<uint8_t> HandleInit();
    std::vector<uint8_t> HandleBestmove(std::vector<Move> moves);

    Move StringToMove(std::vector<uint8_t> str);
    std::vector<uint8_t> MoveToString(const Move& move);

    inline void AppendVectors(std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
    {
        // Inserts b to the end of a.
    }

public:
    UCIInterface() {}
    ~UCIInterface() {}

    std::vector<uint8_t> Respond(const std::vector<uint8_t>& request);
};


