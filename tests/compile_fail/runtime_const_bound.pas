{* EXPECT: constant expression expected *}
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure P(n: Integer); val N2: Integer const := n; val a: array[1..N2] of Integer := default; begin end;
begin end.
