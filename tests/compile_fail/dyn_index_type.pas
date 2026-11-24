{ EXPECT: integer index expected for "array of Integer" }
program T;
val a: array of Integer := default;
begin WriteLn(a['x']); end.
