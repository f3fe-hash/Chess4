# Chess TODOs

The basic board representation, ordinary move generation, castling, promotion,
FEN loading, check/checkmate/stalemate detection, Zobrist hashing, a
transposition table, quiescence search, and initial GoogleTest coverage are in
place. The items below are the remaining work.

## 1. Move generation

Completed:

1. En passant is parsed from FEN, generated, executed, undone, and included in
	Zobrist hashing.
2. Promotions generate queen, rook, bishop, and knight choices.
3. Sliding attacks use generated occupancy-indexed lookup tables built from
	the reference ray scanner.

Remaining:

1. Replace the occupancy-indexed tables with true multiplication-based magic
	bitboards if profiling shows the current tables are not fast enough.
2. Compare the move generator against trusted perft results at increasing
	depths.

## 2. Fix correctness bugs

1. Completed: verified the opening position has 17 legal moves and corrected
	the stale expected count.
2. Add regression tests for pinned pieces, king moves into check, and castling
	rights after rook moves and captures.
3. Compare selected perft positions with trusted reference counts.

## 3. Optimize and clean up

1. Audit functions in all source files and apply `const`, `const_fn`, or
	`pure_fn` from `opt.hpp` only where the function contract permits it.
2. Profile move generation and search, then optimize measured hot paths such
	as repeated attack recomputation without changing public behavior.
3. Refactor duplicated board-state updates and remove comments that merely
	restate the code.

## 4. Expand test coverage

1. Add perft-style tests for castling, checks, pins, and edge-of-board sliding
	attacks; basic move counts and the new promotion/en-passant cases are done.
2. Add tests for repetition tracking and transposition-table replacement.
3. Keep tests deterministic and run them under normal and sanitizer builds.

## 5. Remaining engine work

1. Build a reproducible evaluation position set and tune the documented
	bishop piece-square table against it.
2. Add direct tests for the depth-aware transposition-table replacement policy.
3. Implement proper UCI ponder mode by making search asynchronous, retaining
	the predicted continuation, and handling `stop` and `ponderhit` safely.
