{ EXPECT: borrowed reference 'm1' is used here, but 'a' was used at line 3 while 'm1' held a changeable borrow of it (borrow it as 'const' to share it) }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; var a: TK.Create; val m1, m2: borrowed TK; begin m1 := a; m2 := a; m2.Erhoehe; m1.Erhoehe; end;
begin end.
