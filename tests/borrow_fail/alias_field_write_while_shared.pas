{ EXPECT: borrowed reference 'l' is used here, but 'a.Wert' was changed at line 3 while 'l' was borrowing it (a read-only borrow forbids changes) }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; vao a := TK.Create; var l: TK const := a; begin  a.Wert := 5; WriteLn(l.Info); end;
begin end.
