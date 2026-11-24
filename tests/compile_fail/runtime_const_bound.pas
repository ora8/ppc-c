{ EXPECT: constant expression expected }
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure P(n: Integer); const N2 = n; val a: array[1..N2] of Integer; begin end;
begin end.
