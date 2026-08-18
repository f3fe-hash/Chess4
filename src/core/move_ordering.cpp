#include "core/move_ordering.hpp"


Evaluation MoveOrder::PieceValue(Piece piece)
{
    switch (piece & 0x07)
    {
        case PIECE_TYPE_PAWN:   return PAWN_VALUE;
        case PIECE_TYPE_KNIGHT: return KNIGHT_VALUE;
        case PIECE_TYPE_BISHOP: return BISHOP_VALUE;
        case PIECE_TYPE_ROOK:   return ROOK_VALUE;
        case PIECE_TYPE_QUEEN:  return QUEEN_VALUE;
        case PIECE_TYPE_KING:   return 1000;
        default:                return 0;
    }
}


Evaluation MoveOrder::MoveOrderScore(
    const Move& move,
    const Move& tt_move,
    const int depth)
{
    constexpr Evaluation TT_MOVE_DEPTH_BONUS = 300.0;

    Evaluation score;

    // Don't award too much evaluation to a best move in the transposition
    // table. Although it is probably accurate, award more evaluation based
    // on how deep it was searched.
    if (move == tt_move)
        score += TT_MOVE_DEPTH_BONUS * depth;

    // No piece was captured.
    if (move.captured == NULL_PIECE)
        return 0;

    Evaluation victim = PieceValue(move.captured);
    Evaluation attacker = PieceValue(move.moved);

    // Victim is more valuable than attacker.
    // Setting the value higher will make it trade more pieces, while a lower
    // value will make it choose quiet-er moves. Of course, this is just for
    // ranking. It is not guarrenteed to choose the higher ranked moves, but
    // it will see them earlier on.
    score += victim * 1.3 - attacker;

    return score;
}


bool MoveOrder::CompareMoves(
    const Move& move1,
    const Move& move2,
    const Move& tt_move,
    const int depth)
{
    return MoveOrderScore(move1, tt_move, depth) >
           MoveOrderScore(move2, tt_move, depth);
}


void MoveOrder::OrderMoves(
    std::vector<Move>& moves,
    int depth)
{
    Move tt_move;

    ZobristHash key = board->GetZobristHash();

    if (transposition_table->keyIsStored(key))
    {
        TranspositionTableEntry entry =
            transposition_table->getKey(key);

        if (entry.depth >= depth)
            tt_move = entry.best_move;
    }

    std::sort(
        moves.begin(),
        moves.end(),
        [this, &tt_move, &depth](const Move& a, const Move& b)
        {
            return CompareMoves(a, b, tt_move, depth);
        }
    );
}

