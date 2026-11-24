{ EXPECT: 'Square' is not a compile-time constant: for a value computed at run time write 'val Square: Integer const := ...' }
program T;
procedure P(n: Integer); const Square = n * n; begin WriteLn(Square); end;
begin P(2); end.
