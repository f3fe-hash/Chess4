#pragma once

#include <cstdint>
#include <bit>

// 64 square bitboard.
using Bitboard = uint64_t;

// Note: pieces only actually take up 6 bits.
using Square = uint8_t;

inline Bitboard SquareMask(const Square square)
{
    return Bitboard(1ULL) << square;
}

inline Square GetLSB(const Bitboard bits)
{
    return Square(__builtin_ctzll(bits));
}

inline uint8_t GetNumSetBits(const Bitboard bits)
{
    return std::popcount(bits);
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
