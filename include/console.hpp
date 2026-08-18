#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "chess.hpp"
#include "core/bot.hpp"

// Commands:
// move <from> <to>     - Make a move.
// fen <fen string>     - Load a FEN string.
// timelimit            - Set a time limit for the bot.
// reset                - Reset the chess board.
// board                - Print the chess board.
// bot                  - Make a move from the bot.
// eval                 - Evaluate the current position.
// exit                 - Exit the program.
class Console
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<ChessBot> bot;

    Move ParseMove(std::string move_str);
    void ParseFEN(std::string fen_str);
 
    std::string GetCommand(const std::string& prompt = ">>> ");
    std::vector<std::string> SplitCommand(std::string str);

public:
    Console(std::shared_ptr<ChessBoard> board, std::shared_ptr<ChessBot> bot);
    ~Console();

    std::string MoveToString(Move move);
    void PrintBoard();

    // Prints check / checkmate / stalemate
    void PrintEndgame();
 
    void run();
};
