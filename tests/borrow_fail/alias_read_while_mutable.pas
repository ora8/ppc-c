{ EXPECT: borrowed reference 'm' is used here, but 'a' was used at line 3 while 'm' held a changeable borrow of it (borrow it as 'const' to share it) }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; var a: TK.Create; val m: borrowed TK; begin m := a; WriteLn(a.Info); m.Erhoehe; end;
begin end.
