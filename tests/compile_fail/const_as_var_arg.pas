{* EXPECT: cannot modify constant/loop variable "H" *}
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure Inc2(var a: Integer); begin a := a + 2; end; procedure P(n: Integer); val H: Integer const := n; begin Inc2(H); end;
begin end.
