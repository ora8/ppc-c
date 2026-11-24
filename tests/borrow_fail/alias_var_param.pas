{ EXPECT: borrowed reference 'l' is used after its owner 'a' was passed as var parameter to 'V' at line 3 }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure V(var x: TK); begin end; procedure P; vao a := TK.Create; var l: TK const := a; begin  V(a); WriteLn(l.Info); end;
begin end.
