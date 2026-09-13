#pragma once

#include <vector>

#include "chess.hpp"


inline constexpr int MAX_PLY = 128;
inline constexpr int NUM_KILLERS = 8;

class KillerMoves
{
    Move killer_moves[MAX_PLY][NUM_KILLERS]{};
    int killer_move_count[MAX_PLY]{};

public:
    KillerMoves() = default;
    ~KillerMoves() = default;

    void AddKillerMove(const Move& move, const int ply);

    bool IsKillerMove(const Move& move, const int ply);
};
