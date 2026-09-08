#!/usr/bin/env bash
# run.sh — verifies each script in this directory FAILS (compile or runtime
# error), with an expected message substring. These are negative tests for
# the reified-generics ABI (f<T>(args), separate from value arity) and don't
# belong in the main tests/ suite, which only holds must-succeed scripts.
#
# Usage: ./run.sh [path/to/zen]

set -uo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
ZEN="${1:-$ROOT/../../../bin/zen}"

if [[ ! -x "$ZEN" ]]; then
    echo "zen interpreter not found or not executable: $ZEN" >&2
    exit 1
fi

declare -A EXPECT=(
    [01_missing_generic_syntax.py]="is generic and must be called with"
    [02_generic_call_on_nongeneric.py]="Function is not generic"
    [03_wrong_generic_arity.py]="Wrong number of type arguments"
    [04_non_type_generic_arg.py]="is not a type"
    [05_missing_value_args.py]="expected at least"
    [06_missing_generic_syntax_with_value_args.py]="is generic and must be called with"
    [07_missing_generic_syntax_method.py]="is generic and must be called with"
    [08_generic_init_via_plain_construction.py]="is generic and must be constructed with"
    [09_generic_super_call.py]="cannot be called via super"
    [10_generic_dunder_operator.py]="cannot be invoked this way"
    [11_bitpack_overflow.py]="Too many combined type and value arguments"
)

pass=0
fail=0
for f in "$ROOT"/*.py; do
    name="$(basename "$f")"
    expected="${EXPECT[$name]:-}"
    out="$("$ZEN" "$f" 2>&1)"
    code=$?
    if [[ $code -eq 0 ]]; then
        echo "FAIL  $name — expected a compile/runtime error, but it exited 0"
        fail=$((fail+1))
        continue
    fi
    if [[ -n "$expected" && "$out" != *"$expected"* ]]; then
        echo "FAIL  $name — exited $code (good) but message didn't contain '$expected':"
        echo "      $out"
        fail=$((fail+1))
        continue
    fi
    echo "OK    $name"
    pass=$((pass+1))
done

echo
echo "Passed: $pass  Failed: $fail"
[[ $fail -eq 0 ]]
