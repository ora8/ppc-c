{ EXPECT: const procedure 'P' cannot call the non-const method 'Add' on the non-local 'G' }
program T; type TK = class F: Integer; procedure Add(n: Integer); function Get: Integer; const; end; procedure TK.Add(n: Integer); begin F := F + n; end; function TK.Get: Integer; const; begin Result := F; end; var G: TK.Create;
procedure P; const; begin G.Add(1); end;
begin end.
