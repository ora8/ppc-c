{ EXPECT: "R" has no parent type }
program T;
type R = record X: Integer; procedure P; end;
procedure R.P; begin inherited P; end;
begin end.
