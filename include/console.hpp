#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "chess.hpp"
#include "bot.hpp"

// Commands:
// move <from> <to>     - Make a move
// fen <fen string>     - Load a FEN string
// reset                - Reset the chess board.
// bot                  - Make a move from the bot.
// eval                 - Evaluate the current position.
// exit                 - Exit the program.
class Console
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<ChessBot> bot;

    Move ParseMove(std::string move_str);
    void ParseFEN(std::string fen_str);

    std::string MoveToString(Move move);

    std::string GetCommand();
    std::vector<std::string> SplitCommand(std::string str);

public:
    Console(std::shared_ptr<ChessBoard> board, std::shared_ptr<ChessBot> bot);
    ~Console();

    void run();
};
