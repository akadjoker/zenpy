#!/usr/bin/env bash
# Runs the four texmarks back to back with identical settings and prints
# their RESULT lines. Default: auto-add mode for 25 s each, target 60 fps.
#   ./run_all.sh                      # auto-add, 25 s each
#   ./run_all.sh --fixed 50000 --seconds 10
cd "$(dirname "$0")"
ARGS=("$@")
[[ ${#ARGS[@]} -eq 0 ]] && ARGS=(--seconds 25)
for lang in zen lua lua_fast wren py; do
    if [[ ! -x ./tex_$lang ]]; then
        echo "tex_$lang not built (make)"; continue
    fi
    ./tex_$lang "${ARGS[@]}" | grep '^RESULT'
done
