{ EXPECT: no overload of "P" fits the arguments (Boolean); there are: P(Int64), P(Real) }
program T;
procedure P(i: Int64); overload; begin end; procedure P(r: Real); overload; begin end; procedure Q; begin P(True); end;
begin end.
