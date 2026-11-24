{ EXPECT: const procedure 'P' cannot call the non-const method 'Add' on the non-local 'G' }
program T; type TK = class F: Integer; procedure Add(n: Integer); const function Get: Integer; end; procedure TK.Add(n: Integer); begin F := F + n; end; const function TK.Get: Integer; begin Result := F; end; var G: TK.Create;
const procedure P; begin G.Add(1); end;
begin end.
