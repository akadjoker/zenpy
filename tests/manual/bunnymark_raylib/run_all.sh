#!/usr/bin/env bash
# Runs the four bunnymarks back to back with identical settings and prints
# their RESULT lines. Default: auto-add mode for 25 s each, target 60 fps.
#   ./run_all.sh                      # auto-add, 25 s each
#   ./run_all.sh --fixed 50000 --seconds 10
cd "$(dirname "$0")"
ARGS=("$@")
[[ ${#ARGS[@]} -eq 0 ]] && ARGS=(--seconds 25)
for lang in zen lua wren py; do
    if [[ ! -x ./bunny_$lang ]]; then
        echo "bunny_$lang not built (make)"; continue
    fi
    ./bunny_$lang "${ARGS[@]}" | grep '^RESULT'
done
