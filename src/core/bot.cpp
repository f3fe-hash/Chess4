#include "core/bot.hpp"


#ifdef DEBUG
BotDebugData bot_debug{};

void PrintBotDebug()
{
    std::cout << "[DEBUG] LMR re-searches: "
        << bot_debug.lmr_research_count
        << std::endl;

    std::cout << "[DEBUG] LMR Avg. re-search depth: "
        << std::fixed << std::setprecision(2)
        << (float)bot_debug.total_lmr_research_depth / (float)bot_debug.lmr_research_count
        << std::endl;

    std::cout << "[DEBUG] average % of moves re-searched: "
        << std::fixed << std::setprecision(2)
        << ((float)bot_debug.lmr_research_count / (float)bot_debug.nodes_searched) * 100
        << std::endl;
}

void ClearBotDebug()
{
    bot_debug = BotDebugData{};
}
#endif


ChessBot::ChessBot(std::shared_ptr<ChessBoard> board)
    : ChessBot(board, true)
{}


ChessBot::ChessBot(std::shared_ptr<ChessBoard> board, bool start_manager)
    : board(board), manager(start_manager)
{
    transposition_table = std::make_shared<TranspositionTable>();
    killer_moves = std::make_shared<KillerMoves>();
    move_orderer = std::make_shared<MoveOrder>(transposition_table, killer_moves, board);

    evaluator = ChessBoardEvaluator(board, transposition_table, move_orderer);
}


ChessBot::~ChessBot()
{}


void ChessBot::SetTimeLimit(DurationMs _time_limit)
{
    time_limit_ms.store(_time_limit.count(), std::memory_order_relaxed);
}


static inline float BellCurve(float x)
{
    return exp(-x * x);
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

    const int64_t time =
        (white_to_move ? wtime : btime).count();

    const int64_t opponent_time =
        (white_to_move ? btime : wtime).count();

    const int64_t original_time =
        (white_to_move ? orig_wtime : orig_btime).count();

    const int64_t increment =
        (white_to_move ? winc : binc).count();

    // No time remaining. Return immediately rather than
    // attempting to calculate a normal search time.
    if (time <= 0)
        return DurationMs(1);

    // --------------------------------------------------------
    // Configuration.
    // --------------------------------------------------------

    // Normal minimum and maximum search times.
    //
    // The minimum is only used while we still have enough time
    // for it to be safe. The final safety check below can reduce
    // the actual value.
    constexpr double MINIMUM_TIME = 1.0;
    constexpr double MAXIMUM_TIME = 20.0;

    // Always leave some time on the clock for UCI communication,
    // scheduling, and stopping the search.
    constexpr int64_t SAFETY_MARGIN_MS = 250;

    // --------------------------------------------------------
    // Game phase.
    // --------------------------------------------------------

    const double endgame =
        static_cast<double>(evaluator.GetEndgamePhase()) / 256.0;

    // endgame:
    //
    //   0.0 = opening
    //   0.5 = middlegame
    //   1.0 = endgame
    //
    // Your existing BellCurve() shaping is retained, but the
    // result is used as a multiplier on a base allocation rather
    // than being responsible for the entire time calculation.
    // --------------------------------------------------------

    double phase_multiplier;

    if (endgame > 0.20)
    {
        // Middlegame / endgame.
        //
        // Spend somewhat more time as the position becomes
        // tactically important and material decreases.
        phase_multiplier =
            BellCurve(2.0 * endgame);
    }
    else
    {
        // Opening / early middlegame.
        //
        // Spend less time here because opening positions are
        // generally less dependent on deep calculation.
        phase_multiplier =
            BellCurve(12.0 * (endgame - 1.0));
    }

    // --------------------------------------------------------
    // Base time allocation.
    // --------------------------------------------------------
    //
    // The original time control determines how much time we
    // normally want to spend on a move.
    //
    // Examples:
    //
    //     1 minute  -> ~1 second
    //     2 minutes -> ~1.5 seconds
    //     5 minutes -> ~4 seconds
    //    10 minutes -> ~8 seconds
    //    15 minutes -> ~10-12 seconds
    //
    // sqrt() gives diminishing returns for very long games.
    // --------------------------------------------------------

    const double original_minutes =
        std::max(
            1.0,
            static_cast<double>(original_time) / 60000.0
        );

    double play_time =
        0.85 * std::sqrt(original_minutes);

    // Longer time controls can afford proportionally more
    // thinking, but don't let this grow without bound.
    if (original_minutes >= 10.0)
        play_time *= 1.20;

    if (original_minutes >= 15.0)
        play_time *= 1.10;

    // Apply the position phase.
    play_time *= phase_multiplier;

    // --------------------------------------------------------
    // Remaining-clock adjustment.
    // --------------------------------------------------------
    //
    // This is important:
    //
    // 15 minutes remaining in a 15-minute game is healthy.
    // 15 minutes remaining in a 30-minute game is not.
    //
    // Likewise, 30 seconds remaining in a 1-minute game should
    // cause us to think much less than 30 seconds remaining in
    // a 15-minute game.
    // --------------------------------------------------------

    const double remaining_fraction =
        static_cast<double>(time) /
        static_cast<double>(
            std::max<int64_t>(1, original_time)
        );

    if (remaining_fraction < 0.10)
    {
        // Critically low on time.
        play_time *= 0.30;
    }
    else if (remaining_fraction < 0.20)
    {
        play_time *= 0.45;
    }
    else if (remaining_fraction < 0.35)
    {
        play_time *= 0.65;
    }
    else if (remaining_fraction < 0.50)
    {
        play_time *= 0.85;
    }

    // --------------------------------------------------------
    // Opponent clock adjustment.
    // --------------------------------------------------------
    //
    // If we're significantly behind on time, conserve our clock.
    // If we're significantly ahead, we can afford to calculate
    // longer.
    // --------------------------------------------------------

    if (opponent_time > 0)
    {
        const double clock_ratio =
            static_cast<double>(time) /
            static_cast<double>(opponent_time);

        if (clock_ratio < 0.35)
        {
            play_time *= 0.60;
        }
        else if (clock_ratio < 0.60)
        {
            play_time *= 0.75;
        }
        else if (clock_ratio < 0.80)
        {
            play_time *= 0.90;
        }
        else if (clock_ratio > 2.50)
        {
            play_time *= 1.20;
        }
        else if (clock_ratio > 1.75)
        {
            play_time *= 1.10;
        }
    }

    // --------------------------------------------------------
    // Increment.
    // --------------------------------------------------------
    //
    // Part of the increment can safely be spent on the current
    // move because that time is recovered after making the move.
    // --------------------------------------------------------

    if (increment > 0)
    {
        constexpr double INCREMENT_FRACTION = 0.40;

        play_time +=
            (static_cast<double>(increment) / 1000.0) *
            INCREMENT_FRACTION;
    }

    // --------------------------------------------------------
    // Short-clock protection.
    // --------------------------------------------------------
    //
    // Never allow one move to consume too large a percentage
    // of the remaining clock.
    // --------------------------------------------------------

    double maximum_clock_fraction;

    if (time <= 5000)
        maximum_clock_fraction = 0.10;
    else if (time <= 15000)
        maximum_clock_fraction = 0.08;
    else if (time <= 30000)
        maximum_clock_fraction = 0.07;
    else if (time <= 60000)
        maximum_clock_fraction = 0.06;
    else
        maximum_clock_fraction = 0.05;

    const double clock_limit =
        (static_cast<double>(time) *
         maximum_clock_fraction) / 1000.0;

    play_time = std::min(
        play_time,
        clock_limit
    );

    // --------------------------------------------------------
    // Final limits.
    // --------------------------------------------------------

    play_time = std::clamp(
        play_time,
        MINIMUM_TIME,
        MAXIMUM_TIME
    );

    // Don't use the safety margin if doing so would result in
    // an invalid or negative search time.
    const double safe_time =
        std::max(
            0.001,
            static_cast<double>(
                time - SAFETY_MARGIN_MS
            ) / 1000.0
        );

    play_time = std::min(
        play_time,
        safe_time
    );

    return DurationMs(
        static_cast<int64_t>(play_time * 1000.0)
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
    
    // Is it a promotion?
    if (move.IsPromotion())
        extension += 1;

    return extension;
}


// ------------------------------------------------------------
// SearchCore.
// ------------------------------------------------------------

// LMR values
#define LMR_LOW 1 // Low
#define LMR_MED 2 // Medium
#define LMR_HIG 3 // High
#define LMR_EXT 4 // Extreme

Evaluation ChessBot::SearchCore(const SearchParams& params)
{
    // Unpack the parameters.
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
    bool endgame = evaluator.IsEndgame();

    bool is_tt_move = false;
    const ZobristHash key = board->GetZobristHash();
    if (transposition_table->Contains(key))
    {
        if (transposition_table->GetEntry(key).best_move == move)
            is_tt_move = true;
    }

    // LMR
    if (
        (extension == 0) &&
        (depth >= 3) &&
        move.IsQuietMove() && 
        !is_root_search &&
        !killer_moves->IsKillerMove(move, ply) &&
        !is_tt_move
    )
    {
        if (!endgame)
        {
            if (move_idx >= 120)
                extension -= LMR_EXT;
            else if (move_idx >= 60)
                extension -= LMR_HIG;
            else if (move_idx >= 30)
                extension -= LMR_MED;
            else if (move_idx >= 8)
                extension -= LMR_LOW;
        }
        else
        {
            // Much more conservative LMR for endgames.
            if (move_idx >= 150)
                extension -= LMR_EXT;
            else if (move_idx >= 80)
                extension -= LMR_HIG;
            else if (move_idx >= 50)
                extension -= LMR_MED;
            else if (move_idx >= 20)
                extension -= LMR_LOW;
        }
    }

    // Ensure depth doesn't go negative.
    int search_depth = std::max(0, depth - 1 + extension);

    Evaluation eval =
        MainSearch(
            alpha,
            beta,
            search_depth,
            ply + 1
        );

    // The search was interrupted. Do NOT attempt an LMR re-search.
    if (time_up)
    {
        board->UndoMove(move);
        return 0;
    }

    constexpr Evaluation LMR_RESEARCH_MARGIN = 10;

    const bool was_reduced =
        search_depth < depth - 1;

    bool research = false;

    if (was_reduced)
    {
        // After making the move, the side to move is the opponent.
        //
        // If Black is to move, the parent was maximizing.
        // The move needs a re-search if it appears to improve alpha.
        //
        // If White is to move, the parent was minimizing.
        // The move needs a re-search if it appears to improve
        // (lower) beta.

        if (board->GetTurnColor() == TURN_BLACK)
        {
            research = eval > alpha + LMR_RESEARCH_MARGIN;
        }
        else
        {
            research = eval < beta - LMR_RESEARCH_MARGIN;
        }
    }

    if (research)
    {
    #ifdef DEBUG_LMR_RESEARCH
        bot_debug.lmr_research_count++;
        bot_debug.total_lmr_research_depth += depth;
    #endif

        eval = MainSearch(
            alpha,
            beta,
            depth - 1,
            ply + 1
        );
    }

    board->UndoMove(move);

    return eval;
}


MoveResult ChessBot::EvaluateRootMove(const SearchParams& params)
{
    MoveResult result{};
    result.move = params.move;
    result.eval = SearchCore(params);
    result.nodes_searched = nodes_searched.load(std::memory_order_relaxed);
    return result;
}


// ------------------------------------------------------------
// Root search.
// ------------------------------------------------------------

MoveResult ChessBot::Search(int min_depth, int max_depth)
{
    // Checkmate / stalemate
    if (board->IsCheckMate() || board->IsStaleMate())
    {
        return MoveResult{};
    }

    nodes_searched.store(0, std::memory_order_relaxed);
    time_up = false;
    stop_requested.store(false);
    worker_time_up.store(false);
    search_start = std::chrono::steady_clock::now();

    std::vector<Move> moves = board->GetLegalMoves();

    MoveResult best_move{};

    if (moves.empty())
    {
        best_move.nodes_searched =
            nodes_searched.load(std::memory_order_relaxed);

        return best_move;
    }

    // Give us a legal fallback move in case the time limit
    // expires before the first depth completes.
    best_move.move = moves[0];

    const bool maximizing =
        board->GetTurnColor() == TURN_WHITE;

    int best_depth = 0;

    // --------------------------------------------------------
    // Iterative deepening.
    // --------------------------------------------------------

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

        Evaluation depth_eval =
            maximizing ? INT32_MIN : INT32_MAX;

        MoveResult depth_move = best_move;

        bool depth_completed = true;

        // ----------------------------------------------------
        // Root TT move.
        // ----------------------------------------------------

        Move tt_move{};

        const ZobristHash root_key =
            board->GetZobristHash();

        if (transposition_table->Contains(root_key))
        {
            const auto entry =
                transposition_table->GetEntry(root_key);

            if (entry.depth >= depth)
                tt_move = entry.best_move;
        }
    
#ifdef ORDER_MOVES
        move_orderer->OrderMoves(moves, 0, 0);
#endif

        // ----------------------------------------------------
        // Search root moves sequentially.
        // ----------------------------------------------------

        for (int move_idx = 0;
             move_idx < static_cast<int>(moves.size());
             ++move_idx)
        {
            // Check the time before starting another root move.
            if (stop_requested.load(std::memory_order_relaxed))
            {
                depth_completed = false;
                break;
            }

            const DurationMs time_limit = GetTimeLimit();

            if (time_limit.count() > 0 &&
                std::chrono::steady_clock::now() - search_start
                    >= time_limit)
            {
                time_up = true;
                depth_completed = false;
                break;
            }

#ifndef ORDER_MOVES
            const Move move =
                move_orderer->PickBestMove(
                    moves,
                    move_idx,
                    tt_move,
                    depth
                );
#else
            // Moves are already sorted.
            const Move move = moves[move_idx];
#endif

            SearchParams params = {
                .alpha          = static_cast<Evaluation>(INT32_MIN),
                .beta           = static_cast<Evaluation>(INT32_MAX),
                .depth          = depth,
                .ply            = 0,
                .move           = move,
                .move_idx       = move_idx,
                .is_root_search = true
            };

            // ------------------------------------------------
            // Search this root move directly.
            // No worker thread.
            // No board copy.
            // No JobManager.
            // ------------------------------------------------

            MoveResult result =
                EvaluateRootMove(params);

            if (time_up)
            {
                depth_completed = false;
                break;
            }

            if ((maximizing && result.eval > depth_eval) ||
                (!maximizing && result.eval < depth_eval))
            {
                depth_eval = result.eval;
                depth_move = result;
            }
        }

        // ----------------------------------------------------
        // Only accept a completely searched iteration.
        // ----------------------------------------------------

        const DurationMs depth_time_limit = GetTimeLimit();

        if (stop_requested.load(std::memory_order_relaxed) ||
            time_up ||
            (depth_time_limit.count() > 0 &&
             std::chrono::steady_clock::now() - search_start
                 >= depth_time_limit))
        {
            depth_completed = false;
        }

        if (depth_completed)
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

    // --------------------------------------------------------
    // Final result.
    // --------------------------------------------------------

    best_move.nodes_searched =
        nodes_searched.load(std::memory_order_relaxed);

    best_move.depth = best_depth;

    // Derive mate distance from the final score.
    if (best_move.eval >= CHECKMATE_SCORE - 10000)
    {
        best_move.mate_in_ply =
            CHECKMATE_SCORE - best_move.eval;
    }
    else if (best_move.eval <= -CHECKMATE_SCORE + 10000)
    {
        best_move.mate_in_ply =
            CHECKMATE_SCORE + best_move.eval;
    }
    else
    {
        best_move.mate_in_ply = -1;
    }

    return best_move;
}


// ------------------------------------------------------------
// Main alpha-beta search.
// ------------------------------------------------------------

Evaluation ChessBot::MainSearch(
    Evaluation alpha,
    Evaluation beta,
    int depth,
    int ply
)
{
#ifdef DEBUG
    bot_debug.nodes_searched++;
#endif

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
            // White being mated is bad for white, while black being
            // mated is good for white. Scores are white-centric.
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
        return evaluator.QuiescenceSearch();

    // --------------------------------------------------------
    // Position key.
    // --------------------------------------------------------

    const ZobristHash key = GetPositionKey();

    // --------------------------------------------------------
    // Transposition-table lookup.
    // --------------------------------------------------------

    if (transposition_table->Contains(key))
    {
        const TranspositionTableEntry entry = transposition_table->GetEntry(key);

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

    Move tt_move{};
    if (transposition_table->Contains(key))
    {
        const auto entry = transposition_table->GetEntry(key);
        if (entry.depth >= depth)
            tt_move = entry.best_move;
    }

    move_orderer->OrderMoves(moves, depth, ply);

    Evaluation best_eval;
    Move best_move;

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
#ifndef ORDER_MOVES
            Move move = move_orderer->PickBestMove(
                moves,
                move_idx,
                tt_move,
                depth
            );
#else
            // Moves are already sorted.
            Move move = moves[move_idx];
#endif

            SearchParams params = {
                alpha:          alpha,
                beta:           beta,
                depth:          depth,
                ply:            ply,
                move:           move,
                move_idx:       move_idx,
                is_root_search: false
            };

            Evaluation eval = SearchCore(params);

            if (time_up)
                return 0;

            if (eval > best_eval)
            {
                best_eval = eval;
                best_move = move;
            }
            
            alpha = std::max(alpha, eval);

            // ------------------------------------------------
            // Beta cutoff.
            // ------------------------------------------------

            if (alpha >= beta)
            {
                Evaluation stored_eval =
                    NormalizeMateScore(best_eval, ply);

                transposition_table->SetLowerBound(
                    key,
                    stored_eval,
                    static_cast<uint8_t>(depth)
                );

                if (move.IsQuietMove())
                    killer_moves->AddKillerMove(move, ply);

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
#ifndef ORDER_MOVES
            Move move = move_orderer->PickBestMove(
                moves,
                move_idx,
                tt_move,
                depth
            );
#else
            // Moves are already sorted.
            Move move = moves[move_idx];
#endif

            SearchParams params = {
                alpha:          alpha,
                beta:           beta,
                depth:          depth,
                ply:            ply,
                move:           move,
                move_idx:       move_idx,
                is_root_search: false
            };

            Evaluation eval = SearchCore(params);

            if (time_up)
                return 0;

            if (eval < best_eval)
            {
                best_eval = eval;
                best_move = move;
            }

            beta = std::min(beta, eval);

            // ------------------------------------------------
            // Alpha cutoff.
            // ------------------------------------------------

            if (alpha >= beta)
            {
                Evaluation stored_eval =
                    NormalizeMateScore(best_eval, ply);

                transposition_table->SetUpperBound(
                    key,
                    stored_eval,
                    static_cast<uint8_t>(depth)
                );

                if (move.IsQuietMove())
                    killer_moves->AddKillerMove(move, ply);

                return best_eval;
            }
        }
    }

    // --------------------------------------------------------
    // Exact result.
    // --------------------------------------------------------

    Evaluation stored_eval =
        NormalizeMateScore(best_eval, ply);

    transposition_table->SetExact(
        key,
        stored_eval,
        static_cast<uint8_t>(depth)
    );

    if (!best_move.IsNull())
    {
        transposition_table->SetBestMove(key, best_move, depth);
    }

    return best_eval;
}



// Endgame fens
// fen 8/3P4/8/8/8/6K1/8/7k
// fen 8/8/8/8/8/3k4/8/KR6/


