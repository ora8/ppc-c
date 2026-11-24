{ EXPECT: cannot call procedure 'S' on constant 'w' (it could change it); only functions are allowed }
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure P(v: TV); const w = v; begin w.S(2); end;
begin end.
