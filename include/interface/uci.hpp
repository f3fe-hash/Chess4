#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <sstream>

#include "chess.hpp"
#include "core/bot.hpp"


class UCI
{
    std::shared_ptr<ChessBoard> board;
    std::shared_ptr<ChessBot> bot;

    std::thread search_thread;

    std::atomic<bool> searching{false};
    std::atomic<bool> quit_requested{false};
    std::atomic<bool> pondering{false};

    DurationMs ponder_search_time{0};

    std::mutex board_mutex;
    std::mutex output_mutex;
    std::string pending_output;

    // Search parameters.
    std::int64_t search_depth = 100;
    std::int64_t search_node_limit = 0;

    DurationMs search_time{0};

    DurationMs white_time{0};
    DurationMs black_time{0};

    DurationMs original_white_time{0};
    DurationMs original_black_time{0};

    DurationMs white_increment{0};
    DurationMs black_increment{0};

    bool infinite_search = false;

    // ------------------------------------------------------------
    // Command handlers
    // ------------------------------------------------------------

    std::string HandleUCI();
    std::string HandleReady();
    std::string HandleNewGame();

    std::string HandlePosition(const std::string& command);
    std::string HandleGo(const std::string& command);

    std::string HandleStop();
    std::string HandleQuit();

    std::string HandleSetOption(const std::string& command);
    std::string HandlePonderHit();

    // ------------------------------------------------------------
    // Search
    // ------------------------------------------------------------

    void StartSearch();
    void SearchThread();

    void StopSearch();

    void UpdateSearchTime();

    // ------------------------------------------------------------
    // Position / moves
    // ------------------------------------------------------------

    bool SetPosition(const std::string& command);

    bool ApplyUCIMove(const std::string& string);

    std::string MoveToString(const Move& move) const;

    // Find the actual legal engine move corresponding to UCI notation.
    bool FindLegalMove(const std::string& string, Move& move);

    // ------------------------------------------------------------
    // Utility
    // ------------------------------------------------------------

    static std::vector<std::string> Tokenize(
        const std::string& string);

    static bool IsInteger(const std::string& string);

    std::int64_t UCI::ParseInteger(
        const std::string& string,
        std::int64_t default_value = 0);

public:
    UCI(
        std::shared_ptr<ChessBoard> board,
        std::shared_ptr<ChessBot> bot);

    ~UCI();

    // Process exactly one UCI command.
    //
    // The returned string may contain multiple lines.
    //
    // Commands which begin an asynchronous search return immediately.
    std::string Respond(const std::string& request);

    bool IsSearching() const
    {
        return searching.load();
    }

    bool QuitRequested() const
    {
        return quit_requested.load();
    }

    std::string TakeOutput();
};

