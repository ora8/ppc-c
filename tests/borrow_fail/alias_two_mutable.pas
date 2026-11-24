{ EXPECT: borrowed reference 'm1' is used here, but 'a' was borrowed again as changeable at line 3 while 'm1' was borrowing it }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; vao a := TK.Create; var m1: TK := a; m2: TK := a; begin   m2.Erhoehe; m1.Erhoehe; end;
begin end.
