{ EXPECT: case expression does not handle 'none' (add it or an 'else') }
program T;
val o: optional Integer := 1;
begin WriteLn(case o of some v: v end); end.
