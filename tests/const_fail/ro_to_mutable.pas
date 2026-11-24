{ EXPECT: read-only reference ("TP const") cannot be used as a changeable "TP" in initializer }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure Z(P: TP const); var m: TP := P; begin  end;
begin end.
