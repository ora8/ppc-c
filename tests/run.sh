#!/bin/bash
# ppc test suite: run tests, borrow-checker errors, compile errors
cd "$(dirname "$0")"
PPC=../ppc
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
shopt -s nullglob   # *.pps: the brace syntax, not in every directory
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "ok    $1"; }
bad() { fail=$((fail+1)); echo "FAIL  $1"; [ -n "$2" ] && echo "$2" | sed 's/^/      /'; }

for f in run/*.pas run/*.pps; do
  n=$(basename "${f%.*}")
  head -1 "$f" | grep -q '^module ' && continue   # modules are imported by the run tests, not run themselves
  if ! out=$($PPC -o "$TMP/$n" "$f" 2>&1); then bad "$f" "$out"; continue; fi
  if diff -u "run/$n.expected" <("$TMP/$n" < /dev/null) > "$TMP/diff"; then ok "$f"; else bad "$f" "$(cat "$TMP/diff")"; fi
done
for d in borrow_fail optional_fail stack_fail const_fail result_fail clone_fail generic_fail proc_fail iface_fail overload_fail set_fail compile_fail; do
  for f in $d/*.pas $d/*.pps; do
    head -1 "$f" | grep -q '^module ' && continue   # imported by a test
    exp=$(sed -n -e '1s/^{\* EXPECT: \(.*\) \*}$/\1/p' -e '1s|^// EXPECT: \(.*\)$|\1|p' "$f")
    out=$($PPC -S -o "$TMP/x" "$f" 2>&1); rc=$?
    if [ $rc -ne 0 ] && grep -qF "$exp" <<<"$out"; then ok "$f"; else bad "$f" "expected: $exp
got: $out"; fi
  done
done
for f in runtime_fail/*.pas; do
  n=$(basename "$f" .pas)
  exp=$(sed -n '1s/^{\* EXPECT: \(.*\) \*}$/\1/p' "$f")
  flags=$(sed -n '2s/^{\* FLAGS: \(.*\) \*}$/\1/p' "$f")
  if ! out=$($PPC $flags -o "$TMP/$n" "$f" 2>&1); then bad "$f" "$out"; continue; fi
  err=$("$TMP/$n" < /dev/null 2>&1 >/dev/null); rc=$?
  if [ $rc -ne 0 ] && grep -qF "$exp" <<<"$err"; then ok "$f"; else bad "$f" "expected: $exp
got (rc=$rc): $err"; fi
done
# shared libraries: each library/<name>.pas (and .pps) becomes lib<Name>.so with <Name>.h; the C programs
# library/<name>_*.c use it (linked, or loaded with dlopen: the .so is their first argument)
if command -v gcc >/dev/null; then
  for f in library/*.pas library/*.pps; do
    lib=$(sed -n 's/^library \([A-Za-z_][A-Za-z0-9_]*\);.*/\1/p' "$f" | head -1)
    b=$(basename "${f%.*}"); d="$TMP/lib_$(basename "$f")"; mkdir -p "$d"
    if ! out=$($PPC -o "$d/lib$lib.so" "$f" 2>&1); then bad "$f" "$out"; continue; fi
    for c in library/${b}_*.c; do
      x="$d/$(basename "$c" .c)"
      if ! out=$(gcc -o "$x" "$c" -I"$d" -L"$d" -l"$lib" -ldl -Wl,-rpath,"$d" 2>&1); then bad "$f: $c" "$out"; continue; fi
      if diff -u "${c%.c}.expected" <("$x" "$d/lib$lib.so" 2>&1) > "$TMP/diff"; then ok "$f: $c"; else bad "$f: $c" "$(cat "$TMP/diff")"; fi
    done
  done
fi
# tools/pas2pps.pps (the converter written in the brace syntax) reproduces every .pps twin of run/*.pas
if out=$($PPC -o "$TMP/pas2pps" ../tools/pas2pps.pps 2>&1); then
  for f in run/*.pas; do
    n=$(basename "$f" .pas)
    [ -f "run/$n.pps" ] || continue
    if "$TMP/pas2pps" -o "$TMP/$n.pps" "$f" >/dev/null && cmp -s "$TMP/$n.pps" "run/$n.pps"; then ok "pas2pps $f"; else bad "pas2pps $f" "differs from run/$n.pps"; fi
  done
else bad "tools/pas2pps.pps" "$out"; fi
# the language server, driven like an editor (needs python3)
if command -v python3 >/dev/null; then
  if out=$(python3 lsp/lsp_test.py "$PPC" 2>&1); then ok "lsp/lsp_test.py"; else bad "lsp/lsp_test.py" "$out"; fi
fi
echo "--- $pass passed, $fail failed"
[ $fail -eq 0 ]
