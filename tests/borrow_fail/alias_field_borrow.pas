{ EXPECT: borrowed reference 'k' is used after its owner 'a.Kind' was reassigned at line 3 }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; vao a := TK.Create; var k: optional TK const := a.Kind; begin a.Kind := TK.Create;  a.Kind := none; if k is some x then WriteLn(x.Info); end;
begin end.
