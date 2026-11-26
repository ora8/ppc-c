{* EXPECT: borrowed reference 'l' is used here, but 'm' was changed at line 3 while 'l' was borrowing it (a read-only borrow forbids changes) *}
program T; type TK = class Value: Integer; Child: optional TK; procedure Increment; function Info: Integer; const; end; procedure TK.Increment; begin Inc(Value); end; function TK.Info: Integer; const; begin Result := Value; end;
procedure P; vao a := TK.Create; var m: TK := a; begin var l: TK const := m; begin m.Increment; WriteLn(l.Info); end; end;
begin end.
