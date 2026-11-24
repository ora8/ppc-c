{ EXPECT: read-only reference ("const TQ") cannot be used as a changeable "TQ" }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
type TQ = class function Me: borrowed TQ; const; end; function TQ.Me: borrowed TQ; const; begin Result := Self; end;
begin end.
