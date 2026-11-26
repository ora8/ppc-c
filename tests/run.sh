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
# modules from other directories: --module-paths (both syntaxes share modpaths/app.expected)
for f in modpaths/*.pas modpaths/*.pps; do
  n=$(basename "$f"); mp="modpaths/lib1:modpaths/lib2"
  if ! out=$($PPC --module-paths "$mp" -o "$TMP/mp_$n" "$f" 2>&1); then bad "$f" "$out"; continue; fi
  if diff -u modpaths/app.expected <("$TMP/mp_$n" < /dev/null) > "$TMP/diff"; then ok "$f"; else bad "$f" "$(cat "$TMP/diff")"; fi
done
# --check of a module file (as an editor does): every module of the tests is fine on its own ...
for f in run/*.pas run/*.pps modpaths/lib*/*.pas modpaths/lib*/*.pps; do
  head -1 "$f" | grep -q '^module ' || continue
  if out=$($PPC --check "$f" 2>&1); then ok "check module $f"; else bad "check module $f" "$out"; fi
done
# ... and the errors of a module are found when it is checked alone
for f in module_fail/*.pas; do
  exp=$(sed -n '1s/^{\* EXPECT: \(.*\) \*}$/\1/p' "$f")
  out=$($PPC --check "$f" 2>&1); rc=$?
  if [ $rc -ne 0 ] && [ -n "$exp" ] && grep -qF "$exp" <<<"$out"; then ok "$f"; else bad "$f" "expected: $exp
got: $out"; fi
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
    # Perfect Pascal programs that use the library: 'external' C functions, --shared-objects
    for p in library/programs/${b}_*.pas library/programs/${b}_*.pps; do
      x="$d/prog_$(basename "$p")"
      if ! out=$($PPC "--shared-objects=$d/lib$lib.so" -o "$x" "$p" 2>&1); then bad "$f: $p" "$out"; continue; fi
      if diff -u "${p%.*}.expected" <(cd / && "$x" 2>&1) > "$TMP/diff"; then ok "$f: $p"; else bad "$f: $p" "$(cat "$TMP/diff")"; fi
    done
    # the same library as a static library (--static): the linked C client and the programs (--static-libraries)
    mkdir -p "$d/static"
    if ! out=$($PPC --static -o "$d/static/lib$lib.a" "$f" 2>&1); then bad "$f --static" "$out"; continue; fi
    for c in library/${b}_client.c; do
      [ -f "$c" ] || continue
      x="$d/static/$(basename "$c" .c)"
      if ! out=$(gcc -o "$x" "$c" -I"$d/static" "$d/static/lib$lib.a" -lm 2>&1); then bad "$f --static: $c" "$out"; continue; fi
      if diff -u "${c%.c}.expected" <("$x" 2>&1) > "$TMP/diff"; then ok "$f --static: $c"; else bad "$f --static: $c" "$(cat "$TMP/diff")"; fi
    done
    for p in library/programs/${b}_*.pas library/programs/${b}_*.pps; do
      x="$d/static/prog_$(basename "$p")"
      if ! out=$($PPC "--static-libraries=$d/static/lib$lib.a" -o "$x" "$p" 2>&1); then bad "$f --static: $p" "$out"; continue; fi
      if diff -u "${p%.*}.expected" <("$x" 2>&1) > "$TMP/diff"; then ok "$f --static: $p"; else bad "$f --static: $p" "$(cat "$TMP/diff")"; fi
    done
  done
  # two Perfect Pascal libraries in one program (their internal names must not collide): shared and static
  d="$TMP/two"; mkdir -p "$d"
  if $PPC -o "$d/libMathLib.so" library/mathlib.pas >/dev/null 2>&1 && $PPC -o "$d/libGreet.so" library/greet.pas >/dev/null 2>&1 &&
     $PPC --static -o "$d/libMathLib.a" library/mathlib.pas >/dev/null 2>&1 && $PPC --static -o "$d/libGreet.a" library/greet.pas >/dev/null 2>&1; then
    for p in library/programs/two_libraries.pas library/programs/two_libraries.pps; do
      for how in "--shared-objects=$d/libMathLib.so;$d/libGreet.so" "--static-libraries=$d/libMathLib.a;$d/libGreet.a"; do
        if ! out=$($PPC "$how" -o "$d/prog" "$p" 2>&1); then bad "$p ${how%%=*}" "$out"; continue; fi
        if diff -u library/programs/two_libraries.expected <("$d/prog" 2>&1) > "$TMP/diff"; then ok "$p ${how%%=*}"; else bad "$p ${how%%=*}" "$(cat "$TMP/diff")"; fi
      done
    done
  else bad "library/greet.pas" "the libraries could not be built"; fi
  # --static only for libraries
  if out=$($PPC --static -o "$TMP/x" run/basics.pas 2>&1); then bad "--static of a program" "no error"; elif grep -qF "the file must start with 'library Name;'" <<<"$out"; then ok "--static of a program"; else bad "--static of a program" "$out"; fi
fi
# ppm: projects with a ppc.ini (copied: their target/ stays out of the repository)
PPM="$(cd .. && pwd)/ppm"
if [ -x "$PPM" ]; then
  cp -r ppm "$TMP/ppm"
  for p in "$TMP"/ppm/*/; do
    n=$(basename "$p")
    if out=$(cd "$p" && "$PPM" test 2>&1); then ok "ppm test $n"; else bad "ppm test $n" "$out"; fi
  done
  if diff -u <(printf 'area of 2 inches.. 25.81\n10 inches......... 25.4\nargument: hello\n') <(cd "$TMP/ppm/app" && "$PPM" run -- hello 2>/dev/null | grep -v '^ *\(Compiling\|Finished\|Running\)') > "$TMP/diff"
  then ok "ppm run app"; else bad "ppm run app" "$(cat "$TMP/diff")"; fi
  # ppm new: every kind of project builds and passes its test
  for how in "" "--pps" "--lib" "--lib --pps" "--static"; do
    d="$TMP/new$(echo "$how" | tr -d ' -')"; mkdir -p "$d"
    if out=$(cd "$d" && "$PPM" new demo-proj $how && cd demo-proj && "$PPM" check && "$PPM" test 2>&1); then ok "ppm new $how"; else bad "ppm new $how" "$out"; fi
  done
  if diff <(echo 'Hello, demo-proj!') <(cd "$TMP/new/demo-proj" && "$PPM" run 2>/dev/null | tail -1) >/dev/null; then ok "ppm run (new)"; else bad "ppm run (new)" "wrong output"; fi
  if out=$(cd "$TMP/new/demo-proj" && "$PPM" build --release 2>&1) && [ -x "$TMP/new/demo-proj/target/release/demo-proj" ] &&
     [ -x "$TMP/new/demo-proj/target/debug/demo-proj" ]; then ok "ppm build --release"; else bad "ppm build --release" "$out"; fi
  # make install into a directory: the installed ppm uses the installed ppc
  if out=$(make -s -C .. install DESTDIR="$TMP/inst" PREFIX=/usr 2>&1) && [ "$(cd "$TMP/new/demo-proj" && "$TMP/inst/usr/bin/ppm" run 2>/dev/null | tail -1)" = "Hello, demo-proj!" ]
  then ok "make install"; else bad "make install" "$out"; fi
  if out=$(cd "$TMP" && "$PPM" build 2>&1); then bad "ppm without project" "no error"; elif grep -qF "no ppc.ini" <<<"$out"; then ok "ppm without project"; else bad "ppm without project" "$out"; fi
  if out=$(cd "$TMP/ppm/units" && "$PPM" run 2>&1); then bad "ppm run (modules)" "no error"; elif grep -qF "has only modules" <<<"$out"; then ok "ppm run (modules)"; else bad "ppm run (modules)" "$out"; fi
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
