{ EXPECT: the type of 'I' is not known yet: assign a value to it first ('I := ...') }
program T;
val I;
begin WriteLn(I); I := 1; end.
