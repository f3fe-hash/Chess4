#include "bot.hpp"


ChessBot::ChessBot(std::shared_ptr<ChessBoard> _board) : board(_board)
{
    transposition_table = std::make_shared<TranspositionTable>();

    evaluator = ChessBoardEvaluation(_board, transposition_table);
}


ChessBot::~ChessBot()
{}


void ChessBot::SetTimeLimit(DurationMs _time_limit)
{
    time_limit = _time_limit;
}


// ------------------------------------------------------------
// Mate-score helpers.
// ------------------------------------------------------------

static inline bool IsMateScore(Evaluation score)
{
    return score >= CHECKMATE_SCORE - 10000 ||
           score <= -CHECKMATE_SCORE + 10000;
}


// Convert a score such as:
//
//     CHECKMATE_SCORE - ply
//
// into a ply-independent value for the transposition table.
//
// Positive mate:
//     score = MATE - ply
//     stored = score + ply = MATE
//
// Negative mate:
//     score = -MATE + ply
//     stored = score - ply = -MATE
//
static inline Evaluation NormalizeMateScore(Evaluation score, int ply)
{
    if (score >= CHECKMATE_SCORE - 10000)
        return score + ply;

    if (score <= -CHECKMATE_SCORE + 10000)
        return score - ply;

    return score;
}


// Convert a normalized TT mate score back to a score for
// the current ply.
static inline Evaluation DenormalizeMateScore(Evaluation score, int ply)
{
    if (score >= CHECKMATE_SCORE - 10000)
        return score - ply;

    if (score <= -CHECKMATE_SCORE + 10000)
        return score + ply;

    return score;
}


// ------------------------------------------------------------
// Piece / move ordering.
// ------------------------------------------------------------



bool ChessBot::CompareMoves(
    const Move& move1,
    const Move& move2)
{
    return evaluator.MoveOrderScore(move1) > evaluator.MoveOrderScore(move2);
}


void ChessBot::SortMoves(std::vector<Move>& moves)
{
    std::sort(
        moves.begin(),
        moves.end(),
        [this](const Move& a, const Move& b)
        {
            return CompareMoves(a, b);
        }
    );
}


// ------------------------------------------------------------
// Depth extension.
// ------------------------------------------------------------

int ChessBot::DepthExtension()
{
    // If the move just played gives check, extend the search.
    if (board->IsCheck())
        return 1;

    return 0;
}


// ------------------------------------------------------------
// SearchCore.
// ------------------------------------------------------------

Evaluation ChessBot::SearchCore(
    Evaluation& alpha,
    Evaluation& beta,
    int depth,
    int ply,
    Move move,
    int move_idx)
{
    // --------------------------------------------------------
    // Make the move.
    // --------------------------------------------------------

    board->MakeMove(move);

    // --------------------------------------------------------
    // Extend checking moves.
    // --------------------------------------------------------

    int extension = DepthExtension();

    if (extension == 0)
    {
        // Do not reduce the first few moves.
        if (move_idx >= 5)
            extension = -1;

        else if (move_idx >= 15)
            extension = -2;

        else if (move_idx >= 35)
            extension = -3;
    }

    int search_depth =
        std::max(0, depth - 1 + extension);

    // We just made a move, so the child position is ply + 1.
    Evaluation eval =
        MainSearch(
            alpha,
            beta,
            search_depth,
            ply + 1
        );

    board->UndoMove(move);

    return eval;
}


// ------------------------------------------------------------
// Root search.
// ------------------------------------------------------------

MoveResult ChessBot::Search(int min_depth, int max_depth)
{
    nodes_searched = 0;
    time_up = false;
    search_start = std::chrono::steady_clock::now();

    std::vector<Move> moves = board->GetLegalMoves();

    MoveResult best_move{};

    if (moves.empty())
    {
        // There is no legal move. There is no move to return.
        best_move.nodes_searched = nodes_searched;
        best_move.depth = 0;
        best_move.eval = 0;

        return best_move;
    }

    SortMoves(moves);

    // Give us a legal fallback move in case the time limit
    // expires before the first depth completes.
    best_move.move = moves[0];

    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    int best_depth = 0;

    // A new search gets a fresh TT.
    //transposition_table->clear();

    // --------------------------------------------------------
    // Iterative deepening.
    // --------------------------------------------------------

    for (int depth = min_depth;
         depth <= max_depth;
         ++depth)
    {
        if (time_limit.count() > 0 &&
            std::chrono::steady_clock::now() - search_start
                >= time_limit)
        {
            break;
        }

        Evaluation alpha = INT32_MIN;
        Evaluation beta  = INT32_MAX;

        Evaluation depth_eval =
            maximizing ? INT32_MIN : INT32_MAX;

        MoveResult depth_move = best_move;

        bool depth_completed = true;

        // ----------------------------------------------------
        // Root moves.
        // ----------------------------------------------------

        for (int move_idx = 0;
             move_idx < static_cast<int>(moves.size());
             ++move_idx)
        {
            if (time_limit.count() > 0 &&
                std::chrono::steady_clock::now() - search_start
                    >= time_limit)
            {
                depth_completed = false;
                break;
            }

            Move move = moves[move_idx];

            // IMPORTANT:
            //
            // The root is ply 0.
            // SearchCore makes the move and calls MainSearch
            // at ply 1.
            //
            Evaluation eval =
                SearchCore(
                    alpha,
                    beta,
                    depth,
                    0,
                    move,
                    move_idx
                );

            if (time_up)
            {
                depth_completed = false;
                break;
            }

            if (maximizing)
            {
                if (eval > depth_eval)
                {
                    depth_eval = eval;

                    depth_move.move = move;
                    depth_move.eval = eval;
                }

                alpha = std::max(alpha, eval);
            }
            else
            {
                if (eval < depth_eval)
                {
                    depth_eval = eval;

                    depth_move.move = move;
                    depth_move.eval = eval;
                }

                beta = std::min(beta, eval);
            }

            // Normally this cannot happen at the root because
            // alpha starts at -INF and beta starts at +INF,
            // but keeping the cutoff is harmless.
            if (alpha >= beta)
                break;
        }

        // Only accept a completely searched iteration.
        if (depth_completed && !time_up)
        {
            best_move = depth_move;
            best_depth = depth;
        }
        else
        {
            break;
        }
    }

    best_move.nodes_searched = nodes_searched;
    best_move.depth = best_depth;

    return best_move;
}


// ------------------------------------------------------------
// Main alpha-beta search.
// ------------------------------------------------------------

Evaluation ChessBot::MainSearch(
    Evaluation alpha,
    Evaluation beta,
    int depth,
    int ply)
{
    ++nodes_searched;

    // --------------------------------------------------------
    // Time control.
    // --------------------------------------------------------

    if ((nodes_searched & 4095) == 0)
    {
        if (time_limit.count() > 0 &&
            std::chrono::steady_clock::now() - search_start
                >= time_limit)
        {
            time_up = true;
            return 0;
        }
    }

    // --------------------------------------------------------
    // Generate legal moves BEFORE evaluating the leaf.
    //
    // This is important because checkmate and stalemate must
    // be recognized even when depth == 0.
    // --------------------------------------------------------

    std::vector<Move> moves = board->GetLegalMoves();

    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    // --------------------------------------------------------
    // Checkmate / stalemate.
    // --------------------------------------------------------

    if (moves.empty())
    {
        if (board->IsCheck())
        {
            // Side to move has been checkmated.
            //
            // White is maximizing:
            //     White being mated = very bad.
            //
            // Black is minimizing:
            //     Black being mated = very good.
            //
            // The ply adjustment makes the engine prefer:
            //
            //     fastest mate
            //
            // and avoid:
            //
            //     being mated as quickly as possible.

            if (maximizing)
            {
                return -CHECKMATE_SCORE + ply;
            }
            else
            {
                return CHECKMATE_SCORE - ply;
            }
        }

        // No legal moves and not in check = stalemate.
        return 0;
    }

    // --------------------------------------------------------
    // Leaf evaluation.
    // --------------------------------------------------------

    if (depth <= 0)
        return evaluator.EvaluatePosition();

    // --------------------------------------------------------
    // Position key.
    // --------------------------------------------------------

    const uint64_t key = GetPositionKey();

    // --------------------------------------------------------
    // Transposition-table lookup.
    // --------------------------------------------------------

    if (transposition_table->keyIsStored(key))
    {
        const TranspositionTableEntry& entry = transposition_table->getKey(key);

        if (entry.depth >= depth)
        {
            Evaluation tt_eval =
                DenormalizeMateScore(entry.eval, ply);

            switch (entry.bound)
            {
                case TranspositionTableBound::EXACT:
                    return tt_eval;

                case TranspositionTableBound::LOWER:
                    alpha = std::max(alpha, tt_eval);
                    break;

                case TranspositionTableBound::UPPER:
                    beta = std::min(beta, tt_eval);
                    break;
            }

            if (alpha >= beta)
                return tt_eval;
        }
    }

    // --------------------------------------------------------
    // Move ordering.
    // --------------------------------------------------------

    SortMoves(moves);

    // Save the original alpha/beta values.
    //
    // These are useful for determining the TT bound.
    //const Evaluation original_alpha = alpha;
    //const Evaluation original_beta  = beta;

    Evaluation best_eval;

    // --------------------------------------------------------
    // Maximizing node.
    // --------------------------------------------------------

    if (maximizing)
    {
        best_eval = INT32_MIN;

        for (int move_idx = 0;
             move_idx < static_cast<int>(moves.size());
             ++move_idx)
        {
            Move move = moves[move_idx];

            Evaluation eval =
                SearchCore(
                    alpha,
                    beta,
                    depth,
                    ply,
                    move,
                    move_idx
                );

            if (time_up)
                return 0;

            if (eval > best_eval)
            {
                best_eval = eval;
                transposition_table->setBestMove(board->GetZobristHash(), move, depth);
            }
            
            alpha = std::max(alpha, eval);

            // ------------------------------------------------
            // Beta cutoff.
            // ------------------------------------------------

            if (alpha >= beta)
            {
                Evaluation stored_eval =
                    NormalizeMateScore(best_eval, ply);

                transposition_table->setLowerBound(
                    key,
                    stored_eval,
                    static_cast<uint8_t>(depth)
                );

                return best_eval;
            }
        }
    }

    // --------------------------------------------------------
    // Minimizing node.
    // --------------------------------------------------------

    else
    {
        best_eval = INT32_MAX;

        for (int move_idx = 0;
             move_idx < static_cast<int>(moves.size());
             ++move_idx)
        {
            Move move = moves[move_idx];

            Evaluation eval =
                SearchCore(
                    alpha,
                    beta,
                    depth,
                    ply,
                    move,
                    move_idx
                );

            if (time_up)
                return 0;

            if (eval < best_eval)
            {
                best_eval = eval;
                transposition_table->setBestMove(board->GetZobristHash(), move, depth);
            }

            beta = std::min(beta, eval);

            // ------------------------------------------------
            // Alpha cutoff.
            // ------------------------------------------------

            if (alpha >= beta)
            {
                Evaluation stored_eval =
                    NormalizeMateScore(best_eval, ply);

                transposition_table->setUpperBound(
                    key,
                    stored_eval,
                    static_cast<uint8_t>(depth)
                );

                return best_eval;
            }
        }
    }

    // --------------------------------------------------------
    // Exact result.
    // --------------------------------------------------------

    Evaluation stored_eval =
        NormalizeMateScore(best_eval, ply);

    transposition_table->setExact(
        key,
        stored_eval,
        static_cast<uint8_t>(depth)
    );

    return best_eval;
}



// Endgame fen
// fen 8/3P4/8/8/8/6K1/8/7k