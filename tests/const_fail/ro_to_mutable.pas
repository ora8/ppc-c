{ EXPECT: read-only reference ("const TP") cannot be used as a changeable "TP" in assignment }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure Z(const P: TP); val m: borrowed TP; begin m := P; end;
begin end.
