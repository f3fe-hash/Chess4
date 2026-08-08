#include "chess.hpp"


ChessBoard::ChessBoard()
{}


ChessBoard::~ChessBoard()
{}


void ChessBoard::UpdateOccupancyBitboards()
{
}


void ChessBoard::UpdateAttackBitboards()
{}


void ChessBoard::ComputeAttackLookupBitboards()
{

}


void ChessBoard::MakeMove(Move& move)
{
    if (move.moved == NULL_PIECE)
        move.moved = pieces[move.from];
    
    if (move.captured == NULL_PIECE)
        move.captured = pieces[move.to];
    
    // Promotion
    if (turn == TURN_WHITE)
    {
        if ((move.moved == PIECE_TYPE_PAWN) && (get_piece_y(move.from) == 7))
        {
            // 8th rank. Promotion.
            move.flags &= ~0x07;
            move.flags |= PIECE_TYPE_PAWN; // PIECE_TYPE_QUEEN
            move.moved = PIECE_TYPE_PAWN | PIECE_COLOR_WHITE;  //
        }
    }
    else
    {
        if ((move.moved == PIECE_TYPE_PAWN) && (get_piece_y(move.from) == 1))
        {
            // 1st rank. Promotion.
            move.flags &= ~0x07;
            move.flags |= PIECE_TYPE_PAWN; // PIECE_TYPE_QUEEN
            move.moved = PIECE_TYPE_PAWN | PIECE_COLOR_BLACK;  //
        }
    }

    pieces[move.from] = NULL_PIECE;
    pieces[move.to] = move.moved;

    // Move the piece on the white and black bitboards.
    if (turn == TURN_WHITE)
    {
        occupancy_bitboard_white ^= move.from;
        occupancy_bitboard_black |= move.to;
    }
    else // Black
    {
        occupancy_bitboard_black ^= move.from;
        occupancy_bitboard_white |= move.to;
    }

    // Move the piece on it's own bitboard.
    occupancy_bitboards[move.moved] ^= move.from;
    occupancy_bitboards[move.moved] |= move.to;

    // Remove the piece on the captured bitboard (if there is a piece there).
    if (move.captured)
        occupancy_bitboards[move.captured] ^= move.to;
    
    if (move.moved & 0x07)
    {
        move.moved &= ~0x07;
        move.moved |= PIECE_TYPE_PAWN;
    }
    
    return;
}


void ChessBoard::UndoMove(Move move)
{
    // move.captured and move.moved are already set by MakeMove.

    // Check for promotion flags.
    if (move.flags & 0x07)
    {
        move.moved &= ~0x07;
        move.moved |= PIECE_TYPE_PAWN; // PIECE_TYPE_QUEEN
    }

    pieces[move.from] = move.moved;
    pieces[move.to] = move.captured;

    // Move the piece on the white and black bitboards.
    if (turn == TURN_WHITE)
    {
        occupancy_bitboard_white |= move.from;
        occupancy_bitboard_black ^= move.to;
    }
    else // Black
    {
        occupancy_bitboard_black |= move.from;
        occupancy_bitboard_white ^= move.to;
    }

    // Move the piece on it's own bitboard.
    occupancy_bitboards[move.moved] ^= move.from;
    occupancy_bitboards[move.moved] |= move.to;

    // Remove the piece on the captured bitboard (if there is a piece there).
    if (move.captured)
        occupancy_bitboards[move.captured] ^= move.to;
    
    if (move.moved & 0x07)
    {
        move.moved &= ~0x07;
        move.moved |= PIECE_TYPE_PAWN;
    }
    
    return;
}


void ChessBoard::GetLegalPawnAttacks(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalKnightAttacks(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalBishopAttacks(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalQueenAttacks(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalKingAttacks(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalPawnMoves(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalKnightMoves(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalBishopMoves(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalQueenMoves(std::vector<Move>& moves)
{}


void ChessBoard::GetLegalKingMoves(std::vector<Move>& moves)
{}


std::vector<Move> ChessBoard::GetLegalMoves()
{}


bool ChessBoard::IsCheck()
{}


bool ChessBoard::IsCheckMate()
{}


bool ChessBoard::IsStaleMate()
{}

