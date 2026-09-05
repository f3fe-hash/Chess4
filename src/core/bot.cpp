#include "core/bot.hpp"


ChessBot::ChessBot(std::shared_ptr<ChessBoard> board)
    : ChessBot(board, true)
{}


ChessBot::ChessBot(std::shared_ptr<ChessBoard> board, bool start_manager)
    : board(board), manager(start_manager)
{
    transposition_table = std::make_shared<TranspositionTable>();
    move_orderer = std::make_shared<MoveOrder>(transposition_table, board);

    evaluator = ChessBoardEvaluator(board, transposition_table, move_orderer);
}


ChessBot::~ChessBot()
{}


void ChessBot::SetTimeLimit(DurationMs _time_limit)
{
    time_limit_ms.store(_time_limit.count(), std::memory_order_relaxed);
}


DurationMs ChessBot::CalculateThinkTime(
    const DurationMs wtime,
    const DurationMs btime,
    const DurationMs orig_wtime,
    const DurationMs orig_btime,
    const DurationMs winc,
    const DurationMs binc
)
const
{
    const bool white_to_move =
        board->GetTurnColor() == TURN_WHITE;

    const DurationMs current_time =
        white_to_move ? wtime : btime;

    const DurationMs opponent_time =
        white_to_move ? btime : wtime;

    const DurationMs original_time =
        white_to_move ? orig_wtime : orig_btime;

    const DurationMs increment =
        white_to_move ? winc : binc;

    if (current_time.count() <= 0)
        return DurationMs(1);

    // Determine which part of the game we're in based on
    // how much of our original clock remains.
    double time_remaining_ratio = 1.0;

    if (original_time.count() > 0)
    {
        time_remaining_ratio =
            static_cast<double>(current_time.count()) /
            static_cast<double>(original_time.count());
    }

    double time_fraction = 0.0;

    if (time_remaining_ratio > 2.0 / 3.0)
    {
        // Opening.
        time_fraction = 1.0 / 120.0;
    }
    else if (time_remaining_ratio > 1.0 / 3.0)
    {
        // Middlegame.
        time_fraction = 1.0 / 60.0;
    }
    else
    {
        // Endgame / low clock.
        time_fraction = 1.0 / 240.0;
    }

    auto think_time =
        static_cast<int64_t>(
            current_time.count() * time_fraction);

    // If we're significantly behind on time, be more conservative.
    double clock_ratio = 1.0;

    if (opponent_time.count() > 0)
    {
        clock_ratio =
            static_cast<double>(current_time.count()) /
            static_cast<double>(opponent_time.count());
    }

    if (clock_ratio < 0.5)
        think_time *= 0.75;
    else if (clock_ratio > 2.0)
        think_time *= 1.25;

    // Use part of the increment as additional thinking time.
    //
    // This makes increments useful without allowing the engine
    // to spend the entire increment every move.
    constexpr double INCREMENT_FRACTION = 0.5;

    think_time += static_cast<int64_t>(
        increment.count() * INCREMENT_FRACTION
    );

    constexpr int64_t MIN_THINK_TIME_MS = 50;
    constexpr int64_t MAX_THINK_TIME_MS = 15000;

    think_time = std::clamp(
        think_time,
        MIN_THINK_TIME_MS,
        MAX_THINK_TIME_MS
    );

    // Leave a small safety margin on the clock.
    think_time = std::min(
        think_time,
        std::max<int64_t>(1, current_time.count() - 10)
    );

    return DurationMs(
        std::max<int64_t>(1, think_time)
    );
}


// ------------------------------------------------------------
// Mate-score helpers.
// ------------------------------------------------------------

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
// Depth extension.
// ------------------------------------------------------------

int ChessBot::DepthExtension(const Move& move)
{
    int extension = 0;

    // If the move just played gives check, extend the search.
    if (board->IsCheck())
        extension += 1;
    
    // Is it a promption?
    if (move.flags == MOVE_PROMOTION)
        extension += 1;

    return extension;
}


// ------------------------------------------------------------
// SearchCore.
// ------------------------------------------------------------

Evaluation ChessBot::SearchCore(SearchParams& params)
{
    const Evaluation alpha      = params.alpha;
    const Evaluation beta       = params.beta;
    const int depth             = params.depth;
    const int ply               = params.ply;
    Move move                   = params.move;
    const int move_idx          = params.move_idx;
    const bool is_root_search   = params.is_root_search;

    // --------------------------------------------------------
    // Make the move.
    // --------------------------------------------------------

    board->MakeMove(move);

    // --------------------------------------------------------
    // Extend checking moves.
    // --------------------------------------------------------

    int extension = DepthExtension(move);

    // LMR values
#define LMR_LOW -1 // Low
#define LMR_MED -2 // Medium
#define LMR_HIG -3 // High
#define LMR_EXT -3 // Extreme - disabled for now


    // LMR
    bool endgame = evaluator.IsEndgame();
    if ((extension == 0) && !is_root_search && depth >= 4)
    {
        if (!endgame)
        {
            if (move_idx >= 120)
                extension += LMR_EXT;
            else if (move_idx >= 60)
                extension += LMR_HIG;
            else if (move_idx >= 30)
                extension += LMR_MED;
            else if (move_idx >= 8)
                extension += LMR_LOW;
        }
        else
        {
            // Much more conservative LMR for endgames.
            if (move_idx >= 150)
                extension += LMR_EXT;
            else if (move_idx >= 80)
                extension += LMR_HIG;
            else if (move_idx >= 50)
                extension += LMR_MED;
            else if (move_idx >= 20)
                extension += LMR_LOW;
        }
    }

    // Ensure depth doesn't go negative.
    int search_depth =
        std::max(0, depth - 1 + extension);

    // We just made a move, so the child position is ply + 1.
    Evaluation eval =
        MainSearch(
            alpha,
            beta,
            search_depth,
            ply + 1,
            params.mate_in
        );

    board->UndoMove(move);

    return eval;
}


// ------------------------------------------------------------
// Root search.
// ------------------------------------------------------------

MoveResult ChessBot::Search(int min_depth, int max_depth)
{
    // Checkmate / stalemate
    if (board->IsCheckMate() || board->IsStaleMate())
    {
        // Auto-initialized to default values
        MoveResult result{};

        return result;
    }

    nodes_searched.store(0, std::memory_order_relaxed);
    time_up = false;
    stop_requested.store(false);
    worker_time_up.store(false);
    search_start = std::chrono::steady_clock::now();

    std::vector<Move> moves = board->GetLegalMoves();

    // Auto-initialized to default values
    MoveResult best_move{};

    if (moves.empty())
    {
        // There is no legal move. There is no move to return.
        best_move.nodes_searched = nodes_searched;

        return best_move;
    }

    // Give us a legal fallback move in case the time limit
    // expires before the first depth completes.
    best_move.move = moves[0];

    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    int best_depth = 0;

    // --------------------------------------------------------
    // Iterative deepening.
    // --------------------------------------------------------

    int64_t mate_in_ply = -2;

    for (int depth = min_depth;
         depth <= max_depth;
         ++depth)
    {
        const DurationMs current_time_limit = GetTimeLimit();
        if (current_time_limit.count() > 0 &&
            std::chrono::steady_clock::now() - search_start
            >= current_time_limit)
        {
            break;
        }

        move_orderer->OrderMoves(moves, 0);

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
            const DurationMs current_time_limit = GetTimeLimit();
            if (current_time_limit.count() > 0 &&
                std::chrono::steady_clock::now() - search_start
                    >= current_time_limit)
            {
                depth_completed = false;
                break;
            }

            SearchParams params = {
                alpha:          alpha,
                beta:           beta,
                depth:          depth,
                ply:            0,
                move:           moves[move_idx],
                move_idx:       move_idx,
                is_root_search: true,
                mate_in:        -2
            };

            const Evaluation eval = SearchCore(params);

            if (time_up)
            {
                depth_completed = false;
                break;
            }

            if (params.mate_in < mate_in_ply)
                mate_in_ply = params.mate_in;

            if (maximizing)
            {
                if (eval > depth_eval)
                {
                    depth_eval = eval;

                    depth_move.move = moves[move_idx];
                    depth_move.eval = eval;
                }

                alpha = std::max(alpha, eval);
            }
            else
            {
                if (eval < depth_eval)
                {
                    depth_eval = eval;

                    depth_move.move = moves[move_idx];
                    depth_move.eval = eval;
                }

                beta = std::min(beta, eval);
            }

        }

        // Only accept a completely searched iteration.
        if (depth_completed && !time_up)
        {
            best_move = depth_move;
            best_depth = depth;

            best_move.eval = depth_eval;
        }
        else
        {
            break;
        }
    }

    best_move.nodes_searched =
        nodes_searched.load(std::memory_order_relaxed);
    best_move.depth = best_depth;
    best_move.mate_in_ply = mate_in_ply;

    return best_move;
}


// ------------------------------------------------------------
// Main alpha-beta search.
// ------------------------------------------------------------

Evaluation ChessBot::MainSearch(
    Evaluation alpha,
    Evaluation beta,
    int depth,
    int ply,
    int& mate_in_ply
)
{
    const uint64_t current_node =
        nodes_searched.fetch_add(1, std::memory_order_relaxed) + 1;

    // --------------------------------------------------------
    // Time control.
    // --------------------------------------------------------

    if ((current_node & 4095) == 0)
    {
        if (stop_requested.load() ||
            (external_stop_requested != nullptr &&
             external_stop_requested->load()))
        {
            time_up = true;
            return 0;
        }

        const DurationMs current_time_limit = GetTimeLimit();
        if (current_time_limit.count() > 0)
        {
            auto elapsed =
                std::chrono::duration_cast<DurationMs>(
                    std::chrono::steady_clock::now() -
                    search_start);

            if (elapsed >= current_time_limit)
            {
                time_up = true;
                return 0;
            }
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

            if (ply < mate_in_ply)
                mate_in_ply = ply;

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
        return evaluator.QuiescenceSearch();

    // --------------------------------------------------------
    // Position key.
    // --------------------------------------------------------

    const ZobristHash key = GetPositionKey();

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
                
                // TranspositionTableBound::NONE
                default:
                    break;
            }

            if (alpha >= beta)
                return tt_eval;
        }
    }

    // --------------------------------------------------------
    // Move ordering.
    // --------------------------------------------------------

    move_orderer->OrderMoves(moves, depth);

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

            SearchParams params = {
                alpha:          alpha,
                beta:           beta,
                depth:          depth,
                ply:            ply,
                move:           move,
                move_idx:       move_idx,
                is_root_search: false,
                mate_in:        mate_in_ply
            };

            Evaluation eval = SearchCore(params);

            if (time_up)
                return 0;

            if (eval > best_eval)
            {
                best_eval = eval;
                transposition_table->setBestMove(key, move, depth);
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

            SearchParams params = {
                alpha:          alpha,
                beta:           beta,
                depth:          depth,
                ply:            ply,
                move:           move,
                move_idx:       move_idx,
                is_root_search: false,
                mate_in:        mate_in_ply
            };

            Evaluation eval = SearchCore(params);

            if (time_up)
                return 0;

            if (eval < best_eval)
            {
                best_eval = eval;
                transposition_table->setBestMove(key, move, depth);
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



// Endgame fens
// fen 8/3P4/8/8/8/6K1/8/7k
// fen 8/8/8/8/8/3k4/8/KR6/


