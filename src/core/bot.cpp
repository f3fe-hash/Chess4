#include "core/bot.hpp"





#ifdef DEBUG
BotDebugData bot_debug{};

void PrintBotDebug()
{
    std::cout << "[DEBUG] LMR re-searches: "
        << bot_debug.lmr_research_count
        << std::endl;
    
#ifdef DEBUG_LMR_RESEARCH
    if (bot_debug.lmr_research_count == 0)
    {
        std::cout << "[DEBUG] LMR Avg. re-search depth: 0.00" << std::endl;
    }
    else
    {
        std::cout << "[DEBUG] LMR Avg. re-search depth: "
            << std::fixed << std::setprecision(2)
            << (float)bot_debug.total_lmr_research_depth / (float)bot_debug.lmr_research_count
            << std::endl;
    }

    if (bot_debug.nodes_searched == 0)
    {
        std::cout << "[DEBUG] average % of moves re-searched: N / A" << std::endl;
    }
    else
    {
        std::cout << "[DEBUG] average % of moves re-searched: "
            << std::fixed << std::setprecision(2)
            << ((float)bot_debug.lmr_research_count / (float)bot_debug.nodes_searched) * 100
            << std::endl;
    }
#endif

#ifdef DEBUG_SEARCH_TERMINATION
    // Track stats for each of these:
    // Completed,
    // TimeUp,
    // TTExact,
    // Checkmate,
    // Stalemate
    const float total_terminations = (float)bot_debug.terminations.size();
    float term_completed = 0;
    float term_timeup = 0;
    float term_ttexact = 0;
    float term_checkmate = 0;
    float term_stalemate = 0;
    for (const SearchTermination term : bot_debug.terminations)
    {
        switch (term)
        {
            case SearchTermination::Completed: term_completed++; break;
            case SearchTermination::TimeUp: term_timeup++; break;
            case SearchTermination::TTExact: term_ttexact++; break;
            case SearchTermination::Checkmate: term_checkmate++; break;
            case SearchTermination::Stalemate: term_stalemate++; break;
        }
    }

    if (total_terminations)
    {
        std::cout << "Termination percentages (" << total_terminations << " terminations)" << std::endl;

        std::cout
            << std::fixed << std::setprecision(2)
            << "Termination via completion: " << (term_completed / total_terminations) * 100 << "%"
            << std::endl;

        std::cout
            << std::fixed << std::setprecision(2)
            << "Termination via time up: " << (term_timeup / total_terminations) * 100 << "%"
            << std::endl;

        std::cout
            << std::fixed << std::setprecision(2)
            << "Termination via TT exact: " << (term_ttexact / total_terminations) * 100 << "%"
            << std::endl;

        std::cout
            << std::fixed << std::setprecision(2)
            << "Termination via checkmate: " << (term_checkmate / total_terminations) * 100 << "%"
            << std::endl;

        std::cout
            << std::fixed << std::setprecision(2) <<
            "Termination via stalemate: " << (term_stalemate / total_terminations) * 100 << "%"
            << std::endl;
    }
#endif
}

void ClearBotDebug()
{
    bot_debug = BotDebugData{};
}

// ------------------------------------------------------------
// Search safety / diagnostic guard rails.
// ------------------------------------------------------------
//
// These checks do NOT modify search behavior. They only report
// suspicious completed searches so unexpected TT/Zobrist/
// MakeMove/UndoMove/search-state problems are easier to spot.
//
// In particular, a very deep search completing with very few
// nodes can indicate that the search is unexpectedly receiving
// TT cutoffs or otherwise skipping large portions of the tree.

constexpr int GUARDRAIL_DEPTH_WARNING = 100;
constexpr uint64_t GUARDRAIL_LOW_NODE_THRESHOLD = 200'000;
constexpr int GUARDRAIL_LOW_NODE_MIN_DEPTH = 8;

// A large node-count increase between consecutive completed
// iterations is worth reporting. This is intentionally loose
// because normal alpha-beta behavior can produce large changes
// between depths.
// On average, there are ~30 moves in a chess position. We
// should be alerted when there is way more than that. However,
// LMR and alpha-beta pruning deals with some nodes, but not all.
// If it is an over ~100 move spike, we should be concerned, or at
// least notified.
constexpr double GUARDRAIL_NODE_SPIKE_RATIO = 100.0;

// Don't warn past this. Occasionally there will be massive
// spikes (ex. 1000x) for no reason. Remove the spikes with this.
constexpr double GUARDRAIL_MAX_NODE_SPIKE_RATIO = 300.0;

void CheckSearchGuardRails(
    const int depth,
    const int max_depth,
    const uint64_t nodes,
    const Evaluation eval,
    const DurationMs elapsed,
    const ZobristHash position_key,
    const uint64_t previous_nodes
)
{
    // --------------------------------------------------------
    // Impossible / extremely suspicious node counts.
    // --------------------------------------------------------

    if (depth > 0 && nodes == 0)
    {
        std::cerr
            << "[GUARDRAIL] WARNING: completed depth "
            << depth
            << " with ZERO nodes"
            << " | max_depth=" << max_depth
            << " | eval=" << eval
            << " | hash=" << position_key
            << std::endl;
    }

    // --------------------------------------------------------
    // Extremely deep search.
    // --------------------------------------------------------

    // If it is a forced mate path, don't warn.
    const bool is_mate =
        std::abs(eval) >= CHECKMATE_SCORE - 1000;

    if (depth >= GUARDRAIL_DEPTH_WARNING && !is_mate)
    {
        std::cerr
            << "[GUARDRAIL] WARNING: very deep search completed"
            << " | depth=" << depth
            << " | nodes=" << nodes
            << " | eval=" << eval
            << " | time=" << elapsed.count() << "ms"
            << " | hash=" << position_key
            << std::endl;
    }

    // --------------------------------------------------------
    // Deep search with suspiciously few nodes.
    // --------------------------------------------------------

    if (
        depth >= GUARDRAIL_LOW_NODE_MIN_DEPTH &&
        nodes < GUARDRAIL_LOW_NODE_THRESHOLD &&
        !is_mate
    )
    {
        std::cerr
            << "[GUARDRAIL] WARNING: suspiciously low node count"
            << " | depth=" << depth
            << " | nodes=" << nodes
            << " | threshold=" << GUARDRAIL_LOW_NODE_THRESHOLD
            << " | eval=" << eval
            << " | time=" << elapsed.count() << "ms"
            << " | hash=" << position_key
            << std::endl;
    }

    // --------------------------------------------------------
    // Especially suspicious combination:
    //
    //     depth >= 100
    //     nodes < 200k
    //
    // Give this its own loud message so it is immediately
    // visible in a long engine log.
    // --------------------------------------------------------

    if (
        depth >= GUARDRAIL_DEPTH_WARNING &&
        nodes < GUARDRAIL_LOW_NODE_THRESHOLD
    )
    {
        if (is_mate)
        {
            std::cout
                << "[GUARDRAIL] deep mate search | "
                << "depth=" << depth
                << " | nodes=" << nodes
                << " | eval=" << eval
                << '\n';
        }
        else
        {
            std::cerr
                << "\n"
                << "============================================================\n"
                << "[GUARDRAIL] CRITICAL SEARCH ANOMALY\n"
                << "Depth " << depth
                << " completed with only " << nodes << " nodes.\n"
                << "This is below the " << GUARDRAIL_LOW_NODE_THRESHOLD
                << "-node safety threshold.\n"
                << "Position hash: " << position_key << "\n"
                << "Evaluation: " << eval << "\n"
                << "Elapsed: " << elapsed.count() << " ms\n"
                << "============================================================\n"
                << std::endl;
        }
    }

    // --------------------------------------------------------
    // Sudden node-count explosion between iterations.
    // --------------------------------------------------------

    if (previous_nodes > 0)
    {
        const double ratio =
            static_cast<double>(nodes) /
            static_cast<double>(previous_nodes);

        if (
            ratio >= GUARDRAIL_NODE_SPIKE_RATIO &&
            ratio <= GUARDRAIL_MAX_NODE_SPIKE_RATIO
        )
        {
            std::cerr
                << "[GUARDRAIL] WARNING: large node-count increase"
                << " | depth=" << depth
                << " | previous_nodes=" << previous_nodes
                << " | nodes=" << nodes
                << " | ratio=" << ratio << "x"
                << " | eval=" << eval
                << std::endl;
        }
    }
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

    if (time <= 0)
        return DurationMs(1);

    // --------------------------------------------------------
    // Configuration
    // --------------------------------------------------------

    // Minimum useful search time under normal circumstances.
    constexpr double MINIMUM_TIME_SECONDS = 0.05;

    // Never intentionally spend more than this on one move.
    constexpr double MAXIMUM_TIME_SECONDS = 20.0;

    // Keep this much time in reserve for UCI communication,
    // scheduling delays, and stopping the search.
    constexpr int64_t SAFETY_MARGIN_MS = 250;

    // --------------------------------------------------------
    // Convert times to seconds.
    // --------------------------------------------------------

    const double time_seconds =
        static_cast<double>(time) / 1000.0;

    const double original_seconds =
        static_cast<double>(
            std::max<int64_t>(1, original_time)
        ) / 1000.0;

    const double increment_seconds =
        static_cast<double>(increment) / 1000.0;

    // --------------------------------------------------------
    // Game phase
    // --------------------------------------------------------

    const double phase =
        static_cast<double>(
            evaluator.GetEndgamePhase()
        ) / 256.0;

    // Opening -> less time.
    // Middlegame -> normal time.
    // Endgame -> more time.
    //
    // Keep this relatively mild because material alone does not
    // necessarily indicate tactical complexity.
    double phase_multiplier;

    if (phase < 0.25)
    {
        phase_multiplier = 0.80;
    }
    else if (phase < 0.50)
    {
        phase_multiplier = 0.95;
    }
    else if (phase < 0.75)
    {
        phase_multiplier = 1.05;
    }
    else
    {
        phase_multiplier = 1.20;
    }

    // --------------------------------------------------------
    // Calculate the remaining time budget.
    // --------------------------------------------------------
    //
    // We don't want to spend the entire clock. A reserve is
    // necessary because the engine cannot predict exactly how
    // many moves the game will last.
    //
    // The reserve is smaller in very short games because there
    // simply isn't much time available.
    // --------------------------------------------------------

    //double reserve_fraction;

    //if (original_seconds <= 60.0)
    //{
    //    reserve_fraction = 0.10;
    //}
    //else if (original_seconds <= 300.0)
    //{
    //    reserve_fraction = 0.12;
    //}
    //else
    //{
    //    reserve_fraction = 0.15;
    //}

    //const double reserve =
    //    original_seconds * reserve_fraction;

    //const double usable_time =
    //    std::max(
    //        0.0,
    //        time_seconds - reserve
    //    );

    // --------------------------------------------------------
    // Determine how much time we should normally spend per move.
    // --------------------------------------------------------
    //
    // This is based on the ORIGINAL time control rather than
    // simply dividing the current clock.
    //
    // That means:
    //
    //   1+0   -> roughly 1 second
    //   2+0   -> roughly 2 seconds
    //   5+0   -> roughly 4-5 seconds
    //   10+0  -> roughly 8-10 seconds
    //   15+0  -> roughly 10-15 seconds
    //
    // Longer controls deliberately get diminishing returns.
    // --------------------------------------------------------

    double base_time;

    if (original_seconds <= 60.0)
    {
        base_time = original_seconds / 60.0;
    }
    else
    {
        base_time =
            std::sqrt(original_seconds / 60.0);
    }

    // Scale so that 10-15 minute games get substantially more
    // time than 1-minute games.
    base_time *= 1.0;

    // --------------------------------------------------------
    // Account for how much of our clock remains.
    // --------------------------------------------------------
    //
    // If we're still near the starting clock, use the normal
    // allocation.
    //
    // If we've burned a significant amount of our clock, become
    // increasingly conservative.
    // --------------------------------------------------------

    const double remaining_fraction =
        time_seconds / std::max(1.0, original_seconds);

    double clock_multiplier = 1.0;

    if (remaining_fraction < 0.10)
    {
        clock_multiplier = 0.35;
    }
    else if (remaining_fraction < 0.20)
    {
        clock_multiplier = 0.50;
    }
    else if (remaining_fraction < 0.35)
    {
        clock_multiplier = 0.70;
    }
    else if (remaining_fraction < 0.50)
    {
        clock_multiplier = 0.85;
    }

    // --------------------------------------------------------
    // Prevent the engine from spending time too aggressively.
    // --------------------------------------------------------

    double play_time =
        base_time *
        phase_multiplier *
        clock_multiplier;

    // --------------------------------------------------------
    // Increment
    // --------------------------------------------------------
    //
    // Increment is effectively additional time that becomes
    // available after the move.
    //
    // Spend only part of it immediately so that increment games
    // don't cause the engine to think indefinitely.
    // --------------------------------------------------------

    if (increment_seconds > 0.0)
    {
        constexpr double INCREMENT_FRACTION = 0.40;

        play_time +=
            increment_seconds * INCREMENT_FRACTION;
    }

    // --------------------------------------------------------
    // Clock-ratio adjustment
    // --------------------------------------------------------
    //
    // If we're much behind our opponent, conserve time.
    // If we're far ahead, we can afford deeper searches.
    // --------------------------------------------------------

    if (opponent_time > 0)
    {
        const double opponent_seconds =
            static_cast<double>(opponent_time) / 1000.0;

        const double ratio =
            time_seconds /
            std::max(0.001, opponent_seconds);

        if (ratio < 0.35)
        {
            play_time *= 0.60;
        }
        else if (ratio < 0.60)
        {
            play_time *= 0.75;
        }
        else if (ratio < 0.80)
        {
            play_time *= 0.90;
        }
        else if (ratio > 2.50)
        {
            play_time *= 1.15;
        }
        else if (ratio > 1.75)
        {
            play_time *= 1.08;
        }
    }

    // --------------------------------------------------------
    // Prevent one move from consuming too much of the clock.
    // --------------------------------------------------------

    double maximum_fraction;

    if (time_seconds <= 5.0)
    {
        maximum_fraction = 0.10;
    }
    else if (time_seconds <= 15.0)
    {
        maximum_fraction = 0.08;
    }
    else if (time_seconds <= 30.0)
    {
        maximum_fraction = 0.07;
    }
    else if (time_seconds <= 60.0)
    {
        maximum_fraction = 0.06;
    }
    else
    {
        maximum_fraction = 0.05;
    }

    play_time = std::min(
        play_time,
        time_seconds * maximum_fraction
    );

    // --------------------------------------------------------
    // Final limits
    // --------------------------------------------------------

    play_time = std::clamp(
        play_time,
        MINIMUM_TIME_SECONDS,
        MAXIMUM_TIME_SECONDS
    );

    // Never intentionally use the safety margin.
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
        static_cast<int64_t>(
            play_time * 1000.0
        )
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

    const bool maximizing = board->GetTurnColor() == TURN_WHITE;

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

    const TranspositionTableEntry entry = transposition_table->GetEntry(key, is_tt_move);
    if (is_tt_move)
    {
        if (entry.best_move == move)
            is_tt_move = true;
    }

    int lmr = 0;
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
            // If there are >120 moves in a position, I am assuming some are horrendous.
            if (move_idx >= 120)
                lmr = LMR_EXT;

            else if (move_idx >= 60)
                lmr = LMR_HIG;

            else if (move_idx >= 30)
                lmr = LMR_MED;

            else if (move_idx >= 8)
                lmr = LMR_LOW;
        }
        else
        {
            // Much more conservative LMR for endgames.
            if (move_idx >= 150)
                lmr = LMR_EXT;

            else if (move_idx >= 80)
                lmr = LMR_HIG;

            else if (move_idx >= 50)
                lmr = LMR_MED;

            else if (move_idx >= 20)
                lmr = LMR_LOW;
        }
    }

    // Ensure depth doesn't go negative.
    int search_depth = std::max(0, depth - 1 + extension - lmr);

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

    // Centipawns
    constexpr Evaluation LMR_RESEARCH_MARGIN = 5;

    const bool was_reduced = lmr > 0;

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

        if (maximizing)
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

    uint64_t previous_completed_nodes = 0;

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

        bool found;
        const TranspositionTableEntry move_entry = transposition_table->GetEntry(root_key, found);
        if (found)
        {
            if (move_entry.move_depth >= depth)
                tt_move = move_entry.best_move;
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

            const uint64_t completed_nodes =
                nodes_searched.load(std::memory_order_relaxed);

            const DurationMs elapsed =
                std::chrono::duration_cast<DurationMs>(
                    std::chrono::steady_clock::now() - search_start
                );

#ifdef DEBUG
            CheckSearchGuardRails(
                depth,
                max_depth,
                completed_nodes,
                depth_eval,
                elapsed,
                board->GetZobristHash(),
                previous_completed_nodes
            );
#endif

            previous_completed_nodes = completed_nodes;
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

    // The bot usually can search ~100k-200k NPS. At 200k NPS,
    // it will search ~10k nodes in 5ms. This coarse timing is fine.
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
#ifdef DEBUG_SEARCH_TERMINATION
                bot_debug.terminations.push_back(SearchTermination::TimeUp);
#endif
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

#ifdef DEBUG_SEARCH_TERMINATION
                bot_debug.terminations.push_back(SearchTermination::Checkmate);
#endif

            if (maximizing)
            {
                return -CHECKMATE_SCORE + ply;
            }
            else
            {
                return CHECKMATE_SCORE - ply;
            }
        }

#ifdef DEBUG_SEARCH_TERMINATION
            bot_debug.terminations.push_back(SearchTermination::Stalemate);
#endif

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

    bool found;
    const TranspositionTableEntry lookup_entry = transposition_table->GetEntry(key, found);
    if (found)
    {
        if (lookup_entry.eval_depth >= depth)
        {
            Evaluation tt_eval =
                DenormalizeMateScore(lookup_entry.eval, ply);

            switch (lookup_entry.bound)
            {
                case TranspositionTableBound::EXACT:
#ifdef DEBUG_SEARCH_TERMINATION
                    bot_debug.terminations.push_back(SearchTermination::TTExact);
#endif
                    return tt_eval;

                case TranspositionTableBound::LOWER:
                    alpha = std::max(alpha, tt_eval);
                    if (alpha >= beta)
                        return tt_eval;
                    
                    break;

                case TranspositionTableBound::UPPER:
                    beta = std::min(beta, tt_eval);
                    if (alpha >= beta)
                        return tt_eval;
                    
                    break;
                
                // TranspositionTableBound::NONE
                default:
                    break;
            }
        }
    }

    // --------------------------------------------------------
    // Move ordering.
    // --------------------------------------------------------

    Move tt_move{};
    const TranspositionTableEntry move_entry = transposition_table->GetEntry(key, found);
    if (found)
    {
        if (move_entry.move_depth >= depth)
            tt_move = move_entry.best_move;
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

#ifdef DEBUG_SEARCH_TERMINATION
    bot_debug.terminations.push_back(SearchTermination::Completed);
#endif

    return best_eval;
}



// Endgame fens
// fen 8/3P4/8/8/8/6K1/8/7k
// fen 8/8/8/8/8/3k4/8/KR6/


