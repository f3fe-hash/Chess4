#include "core/move_ordering.hpp"


Evaluation MoveOrder::PieceValue(Piece piece) const
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
    const int depth) const
{
    constexpr Evaluation TT_MOVE_DEPTH_BONUS = 300.0;

    Evaluation score = 0;

    // Don't award too much evaluation to a best move in the transposition
    // table. Although it is probably accurate, award more evaluation based
    // on how deep it was searched.
    if (move == tt_move)
        score += TT_MOVE_DEPTH_BONUS * depth;

    // No piece was captured.
    if (move.captured == NULL_PIECE)
        return score;

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


void MoveOrder::OrderMoves(std::vector<Move>& moves, int depth) const
{
    Move tt_move{};

    const ZobristHash key = board->GetZobristHash();

    if (transposition_table->Contains(key))
    {
        const auto entry = transposition_table->GetEntry(key);

        if (entry.depth >= depth)
            tt_move = entry.best_move;
    }

    struct ScoredMove
    {
        Evaluation score;
        Move move;
    };

    std::vector<ScoredMove> scored;
    scored.reserve(moves.size());

    for (const Move& move : moves)
    {
        scored.push_back({
            MoveOrderScore(move, tt_move, depth),
            move
        });
    }

    std::sort(
        scored.begin(),
        scored.end(),
        [](const ScoredMove& a, const ScoredMove& b)
        {
            return a.score > b.score;
        });

    for (size_t i = 0; i < moves.size(); ++i)
        moves[i] = scored[i].move;
}


Move MoveOrder::PickBestMove(
    std::vector<Move>& moves,
    int start,
    const Move& tt_move,
    int depth) const
{
    int best = start;
    Evaluation best_score = MoveOrderScore(moves[start], tt_move, depth);

    for (int i = start + 1; i < static_cast<int>(moves.size()); ++i)
    {
        Evaluation score = MoveOrderScore(moves[i], tt_move, depth);

        if (score > best_score)
        {
            best_score = score;
            best = i;
        }
    }

    std::swap(moves[start], moves[best]);
    return moves[start];
}

