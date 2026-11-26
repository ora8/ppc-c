{* EXPECT: Runtime error 201: range check error at runtime_fail/range_string.pas(6,13): index 4 is outside 1..3 *}
{* FLAGS: *}
program T;
val s: String := 'abc';
begin
  WriteLn(s[4]);
end.
