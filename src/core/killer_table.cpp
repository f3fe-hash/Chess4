#include "core/killer_table.hpp"


void KillerMoves::AddKillerMove(const Move& move, const int ply)
{
    if (ply < 0 || ply >= MAX_PLY)
        return;

    int& moves_ply = killer_move_count[ply];

    if (moves_ply >= NUM_KILLERS)
        return;

    killer_moves[ply][moves_ply++] = move;
}


bool KillerMoves::IsKillerMove(const Move& move, const int ply)
{
    // Use this for-loop to save on checking a few moves.
    for (int i = 0; i < killer_move_count[ply]; i++)
    {
        const Move killer_move = killer_moves[ply][i];
        if (killer_move == move)
            return true;
    }

    return false;
}

