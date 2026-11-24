{ EXPECT: 'Hoch' is not a compile-time constant: for a value computed at run time write 'val Hoch: Integer const := ...' }
program T;
procedure P(n: Integer); const Hoch = n * n; begin WriteLn(Hoch); end;
begin P(2); end.
