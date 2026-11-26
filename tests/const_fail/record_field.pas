{* EXPECT: cannot modify a part of 'Self', which is constant *}
program T; type TV = record X: Real; procedure P; const; end;
procedure TV.P; const; begin X := 1; end;
begin end.
