{* EXPECT: procedure call used as a value *}
program T;
procedure P; begin end;
val i: Integer := 1;
begin WriteLn(case i of 1: P else 0 end); end.
