# Chess TODOs

The basic board representation, ordinary move generation, castling, promotion,
FEN loading, check/checkmate/stalemate detection, Zobrist hashing, a
transposition table, quiescence search, and initial GoogleTest coverage are in
place. The items below are the remaining work.

## 1. Optimize
Optimize the code. The most used functions (in -Ofast, according to a flamegraph) are:
1. `UpdateAttackBitboards` (47.10 %). In it:
    * `SlidingAttacks` (22.81 %). In it:
        * `CompressedIndex` (22.81 %)
    * `[unknown]` (Called a 8 times recursively) (4.64 %)

2. `ComputeAttackLookupBitboards` (27.03 %). In it:
    * `ComputeSlidingAttacks` (18.37 %), In it:
        * `IsOnBoard` (13.72 %)

3. `[unknown]` (I am assuming `GetLegalMoves`) (22.23 %). In it:
    * `UndoMove` (22.23 %). In it:
        * `GenerateZobristHash` (22.23 %)