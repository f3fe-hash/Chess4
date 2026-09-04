# /bin/sh
set -euo pipefail

ctest chess_tests -V | grep -E '[0-9]+\.[0-9]+ ms'