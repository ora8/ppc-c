{ EXPECT: Runtime error 201: range check error at runtime_fail/range_array.pas(6,28): index 6 is outside 1..5 }
{ FLAGS: }
program T;
val a: array[1..5] of Integer := default;
begin
  for val i := 1 to 6 do a[i] := i;
end.
