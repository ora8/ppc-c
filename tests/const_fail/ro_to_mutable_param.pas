{ EXPECT: read-only reference ("const TP") cannot be used as a changeable "TP" in argument 1 of "F" }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure F(x: TP); begin x.Gb; end; procedure Z(constref P: TP); begin F(P); end;
begin end.
