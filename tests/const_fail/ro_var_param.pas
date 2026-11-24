{ EXPECT: call by var for arg no. 1 has to match exactly }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure V(var a: TP); begin end; procedure Z(const P: TP); val c: borrowed const TP; begin c := P; V(c); end;
begin end.
