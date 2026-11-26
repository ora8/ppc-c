{* EXPECT: cannot call 'S' on constant 'w': only const methods (declared with '; const;') are allowed *}
program T; type TV = record X: Real; procedure S(F: Real); function L: Real; end; procedure TV.S(F: Real); begin X := X * F; end; function TV.L: Real; begin Result := X; end;
procedure P(v: TV); val w: TV const := v; begin w.S(2); end;
begin end.
