# Chess TODOS

## 1. Fix tests
Board tests are broken, and most don't work, because some are wrong, and some show an actual issue in the code.

## 2. Fix depth 100 search bug
Occasionally, in middlegame positions, the engine can search up to depth 100 (the given limit), for no apparent reason. It search around 100k nodes, and it sometimes blunders a piece, but sometimes not. It can occur in tactical positions, but usually just out of nowhere. Very often it gives a "mate in 1" score, which is inaccurate, as there is no mate in one threat.

## 3. Mate score logic
Sometimes, the "mate in X" moves is accurate, however, sometimes, a normal search, not even a depth 100 search will show a mate score, then on the next move it isn't there anymore. Mate scores should be forced mates, not "possible" mates.