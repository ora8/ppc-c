#!/bin/bash
# ppp test suite: run tests, borrow-checker errors, compile errors
cd "$(dirname "$0")"
PPP=../ppp
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "ok    $1"; }
bad() { fail=$((fail+1)); echo "FAIL  $1"; [ -n "$2" ] && echo "$2" | sed 's/^/      /'; }

for f in run/*.pas; do
  n=$(basename "$f" .pas)
  if ! out=$($PPP -o "$TMP/$n" "$f" 2>&1); then bad "$f" "$out"; continue; fi
  if diff -u "run/$n.expected" <("$TMP/$n" < /dev/null) > "$TMP/diff"; then ok "$f"; else bad "$f" "$(cat "$TMP/diff")"; fi
done
for d in borrow_fail compile_fail; do
  for f in $d/*.pas; do
    exp=$(sed -n '1s/^{ EXPECT: \(.*\) }$/\1/p' "$f")
    out=$($PPP -S -o "$TMP/x" "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ] && grep -qF "$exp" <<<"$out"; then ok "$f"; else bad "$f" "expected: $exp
got: $out"; fi
  done
done
echo "--- $pass passed, $fail failed"
[ $fail -eq 0 ]
