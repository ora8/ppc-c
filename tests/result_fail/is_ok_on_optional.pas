{* EXPECT: "optional Integer" is not a result, 'is ok' needs a result value *}
program T;
val o: optional Integer := 1;
begin if o is ok v then WriteLn(v); end.
