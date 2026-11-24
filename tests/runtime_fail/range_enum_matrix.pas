{ EXPECT: Runtime error 201: range check error at runtime_fail/range_enum_matrix.pas(8,10): index 4 is outside 1..3 }
{ FLAGS: }
program T;
type TShade = (Red, Green); TM = array[TShade, 1..3] of Integer;
val m: TM := default; k := 4;
begin
  m[Green, k - 1] := 1;
  m[Red, k] := 2;
end.
