{ EXPECT: read-only parameters are written 'x: T const' (passed by reference, not copied) }
program T;
procedure P(constref x: Integer); begin end;
begin end.
