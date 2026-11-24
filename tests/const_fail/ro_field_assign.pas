{ EXPECT: cannot modify a part of 'P', which is constant }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure Z(P: TP const); begin P.Alter := 1; end;
begin end.
