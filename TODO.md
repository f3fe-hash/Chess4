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

## 4. Fix search logic
Sometimes in games where it is completely losing, the bot will show good scores like 3.00 instead of the more accurate -15.00.

## 5. Implement tuning system
Implement a tuning system for piece values and eval multipliers.

## 6. Real multi-threading
Implement a multi-threaded TT, and have a single bot class (or bot manager class) that uses he single multithreaded TT, and use that one class with the UCI, and one UCI object in the UCI server.