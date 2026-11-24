{ EXPECT: cannot modify constant/loop variable "Limit" }
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
const Limit: Integer = 5; procedure P; begin Limit := 6; end;
begin end.
