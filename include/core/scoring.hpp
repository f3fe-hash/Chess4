#pragma once

using Evaluation = float;

const inline Evaluation CHECKMATE_SCORE = 1e+5;

//
//  Piece values (taken from Stockfish)
//

// Piece values (Opening)
const inline Evaluation PAWN_VALUE_OP   = 100;
const inline Evaluation KNIGHT_VALUE_OP = 300;
const inline Evaluation BISHOP_VALUE_OP = 320;
const inline Evaluation ROOK_VALUE_OP   = 500;
const inline Evaluation QUEEN_VALUE_OP  = 900;

// Piece values (Middlegame)
const inline Evaluation PAWN_VALUE_MG   = 126;
const inline Evaluation KNIGHT_VALUE_MG = 781;
const inline Evaluation BISHOP_VALUE_MG = 825;
const inline Evaluation ROOK_VALUE_MG   = 1276;
const inline Evaluation QUEEN_VALUE_MG  = 2538;

// Piece values (Endgame)
const inline Evaluation PAWN_VALUE_EG    = 208;
const inline Evaluation KNIGHT_VALUE_EG  = 854;
const inline Evaluation BISHOP_VALUE_EG  = 915;
const inline Evaluation ROOK_VALUE_EG    = 1380;
const inline Evaluation QUEEN_VALUE_EG   = 2682;

inline int endgame_phase;

inline void SetEndgamePhase(int phase)
{ endgame_phase = phase; }

inline Evaluation Interpolate(const Evaluation a, const Evaluation b, const float x)
{ return a + x * ( b - a); }

inline Evaluation GetPieceValue(const Evaluation op, const Evaluation mg, const Evaluation eg)
{
    // Gets a piece's value based on the endgame phase.
    
    float phase = endgame_phase;
    if (phase < 128)
    {
        phase = phase / 127;
        return Interpolate(op, mg, phase);
    }
    else
    {
        phase = (phase - 128) / 127;
        return Interpolate(mg, eg, phase);
    }
}

inline Evaluation GetPawnValue()
{ return GetPieceValue(PAWN_VALUE_OP, PAWN_VALUE_MG, PAWN_VALUE_EG); }

inline Evaluation GetKnightValue()
{ return GetPieceValue(KNIGHT_VALUE_OP, KNIGHT_VALUE_MG, KNIGHT_VALUE_EG); }

inline Evaluation GetBishopValue()
{ return GetPieceValue(BISHOP_VALUE_OP, BISHOP_VALUE_MG, BISHOP_VALUE_EG); }

inline Evaluation GetRookValue()
{ return GetPieceValue(ROOK_VALUE_OP, ROOK_VALUE_MG, ROOK_VALUE_EG); }

inline Evaluation GetQueenValue()
{ return GetPieceValue(QUEEN_VALUE_OP, QUEEN_VALUE_MG, QUEEN_VALUE_EG); }


