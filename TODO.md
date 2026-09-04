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
2. Compare deeper positions against trusted perft results when a reference
	harness is available.

## 2. Optimize and clean up

1. Audit functions in all source files and apply `const`, `const_fn`, or
	`pure_fn` from `opt.hpp` only where the function contract permits it.
2. Profile move generation and search, then optimize measured hot paths such
	as repeated attack recomputation without changing public behavior.
3. Refactor duplicated board-state updates and remove comments that merely
	restate the code.

## 3. Expand test coverage

1. Add deeper perft-style tests for castling, checks, pins, and edge-of-board
	sliding attacks; current basic and special-move cases are covered.
2. Completed: added repetition tracking and transposition-table replacement
	tests.
3. Keep tests deterministic and run them under normal and sanitizer builds.

## 4. Remaining engine work

1. Build a reproducible evaluation position set and tune the documented
	bishop piece-square table against it.
2. Add protocol integration coverage using a real TCP client against
	`UCIServer`, including asynchronous `bestmove` delivery.
