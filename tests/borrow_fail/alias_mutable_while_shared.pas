{ EXPECT: borrowed reference 'l' is used here, but 'a' was borrowed again as changeable at line 3 while 'l' was borrowing it }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; vao a := TK.Create; var l: TK const := a; begin var m: TK := a; begin m.Erhoehe; end; WriteLn(l.Info); end;
begin end.
