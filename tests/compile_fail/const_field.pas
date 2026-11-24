{ EXPECT: assignment: cannot modify a part of 'w', which is constant }
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure P(v: TV); const w = v; begin w.X := 1; end;
begin end.
