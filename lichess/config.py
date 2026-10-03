from typing import Optional
import os

LICHESS_API = "https://lichess.org"

LICHESS_TOKEN = os.environ.get("LICHESS_TOKEN")
GROQ_API_KEY = os.environ.get("GROQ_API_KEY")

UCI_HOST = os.environ.get("UCI_HOST", "127.0.0.1")
UCI_PORT = int(os.environ.get("UCI_PORT", "8080"))

# Maximum number of games this process will play simultaneously.
MAX_GAMES = 32

# Challenge policy.
AUTO_ACCEPT_CHALLENGES = True

# Set these according to what you want your bot to accept.
ALLOW_RATED = True
ALLOW_CASUAL = True

# Keep a little time in reserve to avoid losing on time.
MOVE_OVERHEAD_MS = 250

# Never ask the engine to use more than this much time for one move.
# Set to None for no artificial cap.
MAX_MOVE_TIME_MS: Optional[int] = None

# Retry transient failures once per second.
#
# RETRY_COUNT is also the reporting interval. We do NOT give up after
# RETRY_COUNT attempts. Transient connection failures are retried
# indefinitely so a worker can survive a temporary network/server outage.
RETRY_COUNT = 10
RETRY_DELAY_SECONDS = 1

# Backwards-compatible name used by the main event loop.
RECONNECT_DELAY_SECONDS = RETRY_DELAY_SECONDS

# Games that have not had a move for this long are considered stale.
# 24 hours (1 day)
STALE_GAME_SECONDS = 24 * 60 * 60