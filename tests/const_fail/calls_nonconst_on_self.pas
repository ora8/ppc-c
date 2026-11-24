{ EXPECT: cannot call 'Add' on constant 'Self': only const methods }
program T; type TK = class F: Integer; procedure Add(n: Integer); const function Get: Integer; end; procedure TK.Add(n: Integer); begin F := F + n; end;
const function TK.Get: Integer; begin Add(1); Result := F; end;
begin end.
