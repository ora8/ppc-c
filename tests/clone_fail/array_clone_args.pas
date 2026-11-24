{ EXPECT: Clone takes no arguments }
program T;
val a: array[1..2] of Integer := default; b: array[1..2] of Integer := default;
begin b := a.Clone(1); end.
