{ EXPECT: index 0 is outside the array bounds 1..3 }
program T;
val a: array[1..3] of Integer := default;
begin WriteLn(a[0]); end.
