#!/usr/bin/env bash
# Runs the three quadtree demos back to back and prints their RESULT lines.
#   ./run_all.sh                       # --auto, 25 s each
#   ./run_all.sh --fixed 10000 --seconds 5
cd "$(dirname "$0")"
ARGS=("$@")
[[ ${#ARGS[@]} -eq 0 ]] && ARGS=(--auto --seconds 25)
for lang in zen lua wren; do
    if [[ ! -x ./qt_$lang ]]; then
        echo "qt_$lang not built (make)"; continue
    fi
    ./qt_$lang "${ARGS[@]}" | grep '^RESULT'
done
