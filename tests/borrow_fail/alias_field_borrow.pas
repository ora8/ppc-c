{ EXPECT: borrowed reference 'k' is used after its owner 'a.Kind' was reassigned at line 3 }
program T; type TK = class Wert: Integer; Kind: optional TK; procedure Erhoehe; function Info: Integer; const; end; procedure TK.Erhoehe; begin Inc(Wert); end; function TK.Info: Integer; const; begin Result := Wert; end;
procedure P; var a: TK.Create; val k: borrowed const optional TK; begin a.Kind := TK.Create; k := a.Kind; a.Kind := none; if k is some x then WriteLn(x.Info); end;
begin end.
