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
        case PIECE_TYPE_KING:   return CHECKMATE_SCORE;
        default:               return 0;
    }
}

static inline Evaluation MoveOrderScore(const Move& move)
{
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
    Evaluation score1 = MoveOrderScore(move1);
    Evaluation score2 = MoveOrderScore(move2);

    return score1 > score2;
}


void ChessBot::SortMoves(std::vector<Move>& moves)
{
    std::sort(moves.begin(), moves.end(), [this](const Move& a, const Move& b) {
        return CompareMoves(a, b);
    });
}


int ChessBot::DepthExtension()
{
    int extension = 0;
    if (board->IsCheck())
        extension += 1;

    return extension;
}


MoveResult ChessBot::Search(int max_depth)
{
    nodes_searched = 0;
    time_up = false;
    search_start = std::chrono::steady_clock::now();

    std::vector<Move> moves = board->GetLegalMoves();
    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    // Sort the moves.
    SortMoves(moves);

    MoveResult best_move{};
    if (!moves.empty())
        best_move.move = moves[0];

    int best_depth = 0;

    for (int depth = 1; depth <= max_depth; ++depth)
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
        int max_effective_depth = depth;

        for (int move_idx = 0; move_idx < (int)moves.size(); move_idx++)
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
            if ((move_idx >= 5) && (extension == 0))
                extension -= 1;

            int search_depth = std::max(0, depth - 1 + extension);

            max_effective_depth = std::max(max_effective_depth, search_depth);

            Evaluation eval = MainSearch(alpha, beta, search_depth);

            board->UndoMove(move);

            if (time_up)
            {
                depth_completed = false;
                break;
            }

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

                alpha = std::max(depth_eval, alpha);
            }
            else
            {
                if (eval < depth_eval)
                {
                    depth_eval = eval;
                    depth_move.move = move;
                    depth_move.eval = eval;
                }

                beta = std::min(depth_eval, beta);
            }

            if (alpha >= beta)
                break;
        }

        if (!time_up && depth_completed)
        {
            best_move = depth_move;
            best_depth = max_effective_depth;
        }
        else
        {
            break;
        }
    }

    if (best_move.move.from == 0 && best_move.move.to == 0 && !moves.empty())
        best_move.move = moves[0];

    best_move.nodes_searched = nodes_searched;
    best_move.depth = best_depth;
    return best_move;
}


Evaluation ChessBot::SearchCore(Evaluation& alpha, Evaluation& beta, int depth, Move move, int move_idx)
{
    // Core of the search algorithm

    board->MakeMove(move);

    int extension = DepthExtension();
    if (extension == 0)
    {
        // No extension.

        // Low reduction.
        if (move_idx >= 5)
            extension = -1;
        
        // Mid reduction.
        if (move_idx >= 15)
            extension = -2;
        
        // High reduction.
        if (move_idx >= 35)
            extension = -3;
    }
    
    int search_depth = std::max(0, depth - 1 + extension);

    Evaluation eval = MainSearch(alpha, beta, search_depth);

    board->UndoMove(move);

    return eval;
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
    if (moves.size() == 0)
    {
        if (board->IsCheck())
        {
            // High depth left means we found it sooner/closer to the root
            return maximizing ? (-CHECKMATE_SCORE - depth) : (CHECKMATE_SCORE + depth);
        }
        else
            return 0; // draw
    }

    // Leaf node
    if (depth == 0)
    {
        return evaluator.EvaluatePosition();
    }

    // Sort the moves.
    SortMoves(moves);

    //
    //  Main search.
    //
    
    if (maximizing)
    {
        for (int move_idx = 0; move_idx < (int)moves.size(); move_idx++)
        {
            if (time_up) return 0; // Abort up the tree immediately
            
            Move move = moves[move_idx];

            Evaluation eval = SearchCore(alpha, beta, depth, move, move_idx);

            if (eval >= beta)
            {
                return beta; // Fail-high / Prune immediately
            }
            alpha = std::max(alpha, eval);
        }

        return alpha;
    }
    else
    {
        for (int move_idx = 0; move_idx < (int)moves.size(); move_idx++)
        {
            if (time_up) return 0; // Abort up the tree immediately

            Move move = moves[move_idx];

            Evaluation eval = SearchCore(alpha, beta, depth, move, move_idx);

            if (eval <= alpha)
            {
                return alpha; // Fail-low / Prune immediately
            }
            beta = std::min(beta, eval);
        }

        return beta;
    }
}

