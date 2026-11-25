#!/bin/bash
# ppc test suite: run tests, borrow-checker errors, compile errors
cd "$(dirname "$0")"
PPC=../ppc
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "ok    $1"; }
bad() { fail=$((fail+1)); echo "FAIL  $1"; [ -n "$2" ] && echo "$2" | sed 's/^/      /'; }

for f in run/*.pas; do
  n=$(basename "$f" .pas)
  head -1 "$f" | grep -q '^module ' && continue   # modules are imported by the run tests, not run themselves
  if ! out=$($PPC -o "$TMP/$n" "$f" 2>&1); then bad "$f" "$out"; continue; fi
  if diff -u "run/$n.expected" <("$TMP/$n" < /dev/null) > "$TMP/diff"; then ok "$f"; else bad "$f" "$(cat "$TMP/diff")"; fi
done
for d in borrow_fail optional_fail stack_fail const_fail result_fail clone_fail generic_fail proc_fail iface_fail overload_fail compile_fail; do
  for f in $d/*.pas; do
    head -1 "$f" | grep -q '^module ' && continue   # imported by a test
    exp=$(sed -n '1s/^{ EXPECT: \(.*\) }$/\1/p' "$f")
    out=$($PPC -S -o "$TMP/x" "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ] && grep -qF "$exp" <<<"$out"; then ok "$f"; else bad "$f" "expected: $exp
got: $out"; fi
  done
done
for f in runtime_fail/*.pas; do
  n=$(basename "$f" .pas)
  exp=$(sed -n '1s/^{ EXPECT: \(.*\) }$/\1/p' "$f")
  flags=$(sed -n '2s/^{ FLAGS: \(.*\) }$/\1/p' "$f")
  if ! out=$($PPC $flags -o "$TMP/$n" "$f" 2>&1); then bad "$f" "$out"; continue; fi
  err=$("$TMP/$n" < /dev/null 2>&1 >/dev/null); rc=$?
  if [ $rc -ne 0 ] && grep -qF "$exp" <<<"$err"; then ok "$f"; else bad "$f" "expected: $exp
got (rc=$rc): $err"; fi
done
echo "--- $pass passed, $fail failed"
[ $fail -eq 0 ]
