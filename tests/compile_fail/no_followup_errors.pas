{* EXPECT: 1 error(s), compilation aborted *}
program E3;
val x := Unknown;
begin
  WriteLn(x);
  x := 2;
end.
