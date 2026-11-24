{ EXPECT: assignment: cannot modify constant/loop variable "H" }
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure P(n: Integer); const H = n * n; begin H := 1; end;
begin end.
