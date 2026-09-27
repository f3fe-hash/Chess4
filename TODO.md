# Chess TODOS
Every time a TODO is completely done, remove it from this file. When part of a TODO is completed, update the TODO to reflect what still needs to be done. When code it editted, or a TODO is done, use `git commit -m "..."` to commit a change to git. DO NOT PUSH IT.

Going through the process of completing a TODO should look like this:
1. Test the problem in a TODO in debug mode to isolate the problem.
2. Update the TODO to be a full analysis on what happened, and how it should be fixed (if the solution is evident after testing).
3. Start making changes to the code.
4. Update the TODO.
5. Repeat 3-4 until the TODO is completed.
6. Remove TODO from this file.
7. Do a `git commit -m "..."` to reflect what happened.
8. Move on to the next TODO.

## 1. Fix tests
Board tests are broken, and most don't work, because some are wrong, and some show an actual issue in the code.

## 2. Fix depth 100 search bug
Occasionally, in middlegame positions, the engine can search up to depth 100 (the given limit), for no apparent reason. It search around 100k nodes, and it sometimes blunders a piece, but sometimes not. It can occur in tactical positions, but usually just out of nowhere. Very often it gives a "mate in 1" score, which is inaccurate, as there is no mate in one threat.

## 3. Mate score logic
Sometimes, the "mate in X" moves is accurate, however, sometimes, a normal search, not even a depth 100 search, will show a mate score, then on the next move it isn't there anymore. Mate scores should be forced mates, not "possible" mates.

## 4. Fix search logic
Sometimes in games where it is completely losing, the bot will show good scores like 3.00 instead of the more accurate -15.00.

## 5. Implement tuning system
Implement a tuning system for piece values and eval multipliers.