#include "core/move_ordering.hpp"


Evaluation MoveOrder::PieceValue(Piece piece)
{
    // Uses previously set endgame phase.
    switch (piece & 0x07)
    {
        case PIECE_TYPE_PAWN:   return GetPawnValue();
        case PIECE_TYPE_KNIGHT: return GetKnightValue();
        case PIECE_TYPE_BISHOP: return GetBishopValue();
        case PIECE_TYPE_ROOK:   return GetRookValue();
        case PIECE_TYPE_QUEEN:  return GetQueenValue();
        case PIECE_TYPE_KING:   return 1500;
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
    score += victim * 5.4 - attacker;

    return score;
}


void MoveOrder::OrderMoves(std::vector<Move>& moves, int depth)
{
    Move tt_move{};
    ZobristHash key = board->GetZobristHash();

    if (transposition_table->keyIsStored(key))
    {
        auto entry = transposition_table->getKey(key);
        if (entry.depth >= depth)
            tt_move = entry.best_move;
    }

    // Compute score once per move
    struct Scored {
        Move m;
        int score;
    };

    std::vector<Scored> tmp;
    tmp.reserve(moves.size());

    for (const Move& m : moves)
    {
        // Score should incorporate:
        int score = MoveOrderScore(m, tt_move, depth);
        tmp.push_back({m, score});
    }

    std::sort(tmp.begin(), tmp.end(),
              [](const Scored& a, const Scored& b) {
                  return a.score > b.score; // descending
              });

    // Write back
    for (size_t i = 0; i < tmp.size(); ++i)
        moves[i] = tmp[i].m;
}



