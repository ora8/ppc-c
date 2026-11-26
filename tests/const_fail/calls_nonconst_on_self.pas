{* EXPECT: cannot call 'Add' on constant 'Self': only const methods *}
program T; type TK = class F: Integer; procedure Add(n: Integer); function Get: Integer; const; end; procedure TK.Add(n: Integer); begin F := F + n; end;
function TK.Get: Integer; const; begin Add(1); Result := F; end;
begin end.
