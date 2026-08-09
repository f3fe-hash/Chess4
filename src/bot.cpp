#include "bot.hpp"


const int CHECKMATE_SCORE = 1e+9; // 1 billion


ChessBot::ChessBot(std::shared_ptr<ChessBoard> _board) : evaluator(_board), board(_board)
{}


ChessBot::~ChessBot()
{}


void ChessBot::SetTimeLimit(DurationMs _time_limit)
{
    time_limit = _time_limit;
}


Evaluation ChessBot::Evaluate()
{
    return evaluator.EvaluatePosition();
}


static inline Evaluation PieceValue(Piece piece)
{
    switch (piece & 0x07)
    {
        case PIECE_TYPE_PAWN:   return 100;
        case PIECE_TYPE_KNIGHT: return 300;
        case PIECE_TYPE_BISHOP: return 320;
        case PIECE_TYPE_ROOK:   return 500;
        case PIECE_TYPE_QUEEN:  return 900;
        case PIECE_TYPE_KING:   return 1000;
        default:               return 0;
    }
}


static inline Evaluation MoveOrderScore(const Move& move)
{
    // Score how good a move is.
    if (move.captured == NULL_PIECE)
        return 0;
    
    int score = 0;

    Evaluation victim = PieceValue(move.captured);
    Evaluation attacker = PieceValue(move.moved);
    score += (victim * 100) - attacker;

    return score;
}


bool ChessBot::CompareMoves(const Move& move1, const Move& move2)
{
    // Compare 2 moves' evaulation.
    Evaluation score1 = MoveOrderScore(move1);
    Evaluation score2 = MoveOrderScore(move2);

    return score1 > score2;
}


void ChessBot::SortMoves(std::vector<Move>& moves)
{
    // Sort moves by how good they are, so alpha-beta can prune more.
    std::sort(moves.begin(), moves.end(), [this](const Move& a, const Move& b) {
        return CompareMoves(a, b);
    });
}


int ChessBot::DepthExtension()
{
    // Simple depth extension calculation.

    int extension = 0;
    if (board->IsCheck())
        extension += 1;

    return extension;
}


Evaluation ChessBot::SearchCore(Evaluation& alpha, Evaluation& beta, int depth, Move move, int move_idx)
{
    // Core of the search algorithm

#define LOW_REDUCTION   1 // Move indexes [5 - 15)
#define MID_REDUCTION   2 // Move indexes [15 - 35)
#define HIGH_REDUCTION  4 // Move indexes [35 - inf.]

    board->MakeMove(move);

    int extension = DepthExtension();
    if (extension == 0)
    {
        // No depth extension. Add late move reduction.

        // Low reduction.
        if (move_idx >= 5)
            extension = -LOW_REDUCTION;
        
        // Mid reduction.
        if (move_idx >= 15)
            extension = -MID_REDUCTION;
        
        // High reduction.
        if (move_idx >= 35)
            extension = -HIGH_REDUCTION;
    }
    
    int search_depth = std::max(0, depth - 1 + extension);

    Evaluation eval = MainSearch(alpha, beta, search_depth);

    board->UndoMove(move);

    return eval;
}


MoveResult ChessBot::Search(int min_depth, int max_depth)
{
    nodes_searched = 0;
    time_up = false;
    search_start = std::chrono::steady_clock::now();

    std::vector<Move> moves = board->GetLegalMoves();
    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    SortMoves(moves);

    MoveResult best_move{};
    if (!moves.empty())
        best_move.move = moves[0];

    int best_depth = 0;

    // Iterative deepening loop.
    for (int depth = min_depth; depth <= max_depth; ++depth)
    {
        if (time_limit.count() > 0 &&
            std::chrono::steady_clock::now() - search_start >= time_limit)
        {
            break;
        }

        Evaluation alpha = INT32_MIN;
        Evaluation beta = INT32_MAX;

        MoveResult depth_move = best_move;
        Evaluation depth_eval = maximizing ? INT32_MIN : INT32_MAX;

        bool depth_completed = true;

        // Main search loop.
        for (int move_idx = 0; move_idx < (int)moves.size(); ++move_idx)
        {
            if (time_limit.count() > 0 &&
                std::chrono::steady_clock::now() - search_start >= time_limit)
            {
                depth_completed = false;
                break;
            }

            Move move = moves[move_idx];

            board->MakeMove(move);

            int extension = DepthExtension();

            if (extension == 0)
            {
                if (move_idx >= 5)
                    extension = -1;

                if (move_idx >= 15)
                    extension = -2;

                if (move_idx >= 35)
                    extension = -3;
            }

            int search_depth = std::max(0, depth - 1 + extension);

            Evaluation eval = MainSearch(alpha, beta, search_depth);

            board->UndoMove(move);

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

            if (alpha >= beta)
                break;
        }

        if (!time_up && depth_completed)
        {
            best_move = depth_move;
            best_depth = depth;
        }
        else
        {
            break;
        }
    }

    if (best_move.move.from == 0 &&
        best_move.move.to == 0 &&
        !moves.empty())
    {
        best_move.move = moves[0];
    }

    best_move.nodes_searched = nodes_searched;
    best_move.depth = best_depth;

    return best_move;
}


Evaluation ChessBot::MainSearch(Evaluation alpha, Evaluation beta, int depth)
{
    nodes_searched++;

    // Check for time limit
    if (nodes_searched % 4095 == 0)
    {
        if (time_limit.count() > 0 &&
            std::chrono::steady_clock::now() - search_start >= time_limit)
        {
            time_up = true;
            return 0;
        }
    }

    std::vector<Move> moves = board->GetLegalMoves();
    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    // Checkmate / Stalemate
    if (moves.empty())
    {
        if (board->IsCheck())
        {
            return maximizing
                ? (-CHECKMATE_SCORE - depth)
                : (CHECKMATE_SCORE + depth);
        }

        return 0;
    }

    // Leaf node evaluation
    if (depth == 0)
        return evaluator.EvaluatePosition();

    const uint64_t key = GetPositionKey();

    // Transposition-table lookup.
    if (transposition_table.contains(key))
    {
        const TranspositionEntry& entry = transposition_table.at(key);

        if (entry.depth >= depth)
        {
            switch (entry.bound)
            {
                case TranspositionEntry::EXACT:
                    return entry.eval;

                case TranspositionEntry::LOWER_BOUND:
                    alpha = std::max(alpha, entry.eval);
                    break;

                case TranspositionEntry::UPPER_BOUND:
                    beta = std::min(beta, entry.eval);
                    break;
            }

            if (alpha >= beta)
                return entry.eval;
        }
    }

    // Sort the moves.
    SortMoves(moves);

    //
    // Main search.
    //

    Evaluation result;

    if (maximizing)
    {
        for (int move_idx = 0; move_idx < (int)moves.size(); move_idx++)
        {
            Move move = moves[move_idx];

            Evaluation eval =
                SearchCore(alpha, beta, depth, move, move_idx);

            if (time_up)
                return 0;

            if (eval >= beta)
            {
                result = eval;

                transposition_table[key] = {
                    result,
                    (uint8_t)depth,
                    TranspositionEntry::LOWER_BOUND
                };

                return result;
            }

            alpha = std::max(alpha, eval);
        }

        result = alpha;
    }
    else
    {
        for (int move_idx = 0; move_idx < (int)moves.size(); move_idx++)
        {
            Move move = moves[move_idx];

            Evaluation eval =
                SearchCore(alpha, beta, depth, move, move_idx);

            if (time_up)
                return 0;

            if (eval <= alpha)
            {
                result = eval;

                transposition_table[key] = {
                    result,
                    (uint8_t)depth,
                    TranspositionEntry::UPPER_BOUND
                };

                return result;
            }

            beta = std::min(beta, eval);
        }

        result = beta;
    }

    // The search completed without a cutoff, so the result is exact.
    transposition_table[key] = {
        result,
        (uint8_t)depth,
        TranspositionEntry::EXACT
    };

    return result;
}

