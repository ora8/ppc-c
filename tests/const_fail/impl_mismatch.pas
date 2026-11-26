{* EXPECT: "TV.L" is declared 'const': the implementation must match *}
program T; type TV = record X: Real; function L: Real; const; end;
function TV.L: Real; begin Result := X; end;
begin end.
