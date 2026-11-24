{ EXPECT: read-only reference ("TP const") cannot be used as a changeable "TP" in argument 1 of "F" }
program T; type TP = class Age: Integer; Friend: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Age); end; function TP.Info: Integer; const; begin Result := Age; end;
procedure F(x: TP); begin x.Gb; end; procedure Z(P: TP const); begin F(P); end;
begin end.
