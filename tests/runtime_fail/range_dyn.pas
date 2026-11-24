{ EXPECT: Runtime error 201: range check error at runtime_fail/range_dyn.pas(6,13): index 3 is outside 0..2 }
{ FLAGS: }
program T;
val a: array of Integer := (1, 2, 3);
begin
  WriteLn(a[3]);
end.
