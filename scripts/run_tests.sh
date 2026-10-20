#!/bin/bash
cd "$(dirname "$0")/.." || exit 1

BIN=./expltac
DIR=tests/kalpana
OUT=$(mktemp -d)
pass=0
fail=0

for f in "$DIR"/*.tac; do
  case "$f" in *.expected.tac) continue;; esac
  name=$(basename "$f" .tac)
  exp="$DIR/$name.expected.tac"

  if [ ! -f "$exp" ]; then
    echo "SKIP  $name (no expected file)"
    continue
  fi

  if ! $BIN "$f" --tac -o "$OUT" > /dev/null 2>&1; then
    echo "FAIL  $name (program crashed)"
    fail=$((fail + 1))
    continue
  fi

  if diff -q "$OUT/$name.opt.tac" "$exp" > /dev/null; then
    echo "PASS  $name"
    pass=$((pass + 1))
  else
    echo "FAIL  $name"
    diff "$OUT/$name.opt.tac" "$exp"
    fail=$((fail + 1))
  fi
done

rm -rf "$OUT"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
