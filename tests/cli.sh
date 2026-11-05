#!/usr/bin/env bash
set -euo pipefail
bin=${1:?supply CLI path}
mkdir -p build/cli-tests
expected='PASS sheets=1 used_area=54 kerf_area=6'
[[ "$("$bin" demo)" == "$expected" ]]
[[ "$("$bin" plan --input examples/kerf.json --human)" == "$expected" ]]
"$bin" plan --input examples/kerf.json > build/cli-tests/plan.json
[[ "$("$bin" verify --input build/cli-tests/plan.json --human)" == "$expected" ]]
summary='{"ok":true,"summary":{"cuts":1,"kerf_area":6,"placed_items":2,"remaining_area":0,"remaining_leaves":0,"sheets":1,"stock_area":60,"used_area":54}}'
[[ "$("$bin" demo --json)" == "$summary" ]]
[[ "$("$bin" verify --input - < build/cli-tests/plan.json)" == "$summary" ]]
printf '{"stock":{"width":1.0,"height":6},"kerf":1,"sheet_limit":2,"items":[]}' > build/cli-tests/float.json
if "$bin" plan --input build/cli-tests/float.json > build/cli-tests/error.json; then
    printf 'Float token was accepted\n' >&2
    exit 1
else
    [[ $? == 2 ]]
fi
[[ "$(< build/cli-tests/error.json)" == *'"code":"INVALID_JSON"'* ]]
if "$bin" unknown > build/cli-tests/usage.txt 2>&1; then
    exit 1
else
    [[ $? == 64 ]]
fi
printf 'PASS CLI demo, serialization, replay, stdin and rejection\n'
