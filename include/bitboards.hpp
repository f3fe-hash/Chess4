#pragma once

#include <cstdint>

// 64 square bitboard.
using Bitboard = uint64_t;

// Note: pieces only actually take up 6 bits.
using Square = uint8_t;

inline Bitboard SquareMask(const Square square)
{
    return Bitboard(1ULL) << square;
}

inline Square PopLSB(Bitboard& bits)
{
    Square sq = Square(__builtin_ctzll(bits));
    bits &= bits - 1;
    return sq;
}

inline Square PopBitboard(Bitboard bits)
{
    if (!bits)
        return (Square)(64);

    return PopLSB(bits);
}
