#!/usr/bin/env bash
# ./run_all.sh [--n N] [--frames F]   -> RESULT lines for cpp, zen, lua, wren
cd "$(dirname "$0")"
for b in glue_cpp glue_zen glue_lua glue_wren; do
    [[ -x ./$b ]] && ./$b "$@" | grep '^RESULT'
done
