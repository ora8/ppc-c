{* EXPECT: 'else' is unreachable: every case is handled *}
program T;
val o: optional Integer := 1;
begin WriteLn(case o of some v: v; none: 0 else 5 end); end.
