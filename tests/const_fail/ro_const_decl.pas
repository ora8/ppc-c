{ EXPECT: cannot call 'Gb' on constant 'k' }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure Z(x: TP); const k = x; begin k.Gb; end;
begin end.
