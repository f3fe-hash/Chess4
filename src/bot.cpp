#include "bot.hpp"

ChessBot::ChessBot(std::shared_ptr<ChessBoard> _board) : evaluator(_board), board(_board)
{}


ChessBot::~ChessBot()
{}


void ChessBot::SetTimeLimit(DurationMs)
{
    // TODO: Implement.
}


Evaluation ChessBot::Evaluate()
{
    return evaluator.EvaluatePosition();
}


MoveResult ChessBot::Search(int depth)
{
    // Root node.
    nodes_searched = 0;

    std::vector<Move> moves = board->GetLegalMoves();

    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    MoveResult best_move;
    Evaluation best_eval = maximizing ? INT32_MIN : INT32_MAX;
    Evaluation alpha = INT32_MIN;
    Evaluation beta = INT32_MAX;

    for (Move move : moves)
    {
        board->MakeMove(move);
        Evaluation eval = MainSearch(alpha, beta, depth - 1);
        board->UndoMove(move);

        if (maximizing)
        {
            if (best_eval < eval)
            {
                best_eval = eval;
                best_move.move = move;
                best_move.eval = eval;
            }

            alpha = max(best_eval, alpha);
        }
        else
        {
            if (best_eval > eval)
            {
                best_eval = eval;
                best_move.move = move;
                best_move.eval = eval;
            }

            beta = min(best_eval, beta);
        }

        // Prune.
        if (alpha >= beta)
            break;
    }

    best_move.nodes_searched = nodes_searched;

    return best_move;
}


Evaluation ChessBot::MainSearch(Evaluation alpha, Evaluation beta, int depth)
{
    // Main search function (recursive part of search).

    nodes_searched ++;

    if (depth == 0)
    {
        // Leaf node.
        return evaluator.EvaluatePosition();
    }

    std::vector<Move> moves = board->GetLegalMoves();

    bool maximizing = board->GetTurnColor() == TURN_WHITE;

    Evaluation best_eval = maximizing ? INT32_MIN : INT32_MAX;

    for (Move move : moves)
    {
        board->MakeMove(move);
        Evaluation eval = MainSearch(alpha, beta, depth - 1);
        board->UndoMove(move);

        if (maximizing)
        {
            best_eval   = max(best_eval, eval);
            alpha       = max(best_eval, alpha);
        }
        else
        {
            best_eval   = min(best_eval, eval);
            beta        = min(best_eval, beta);
        }

        // Prune.
        if (alpha >= beta)
            break;
    }

    return best_eval;
}

