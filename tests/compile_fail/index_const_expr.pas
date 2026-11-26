{* EXPECT: index 10 is outside the array bounds 1..3 *}
program T;
const N = 3;
val a: array[1..N] of Integer := default;
begin a[N * 3 + 1] := 1; end.
