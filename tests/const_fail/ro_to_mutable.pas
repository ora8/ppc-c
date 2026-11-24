{ EXPECT: read-only reference ("TP const") cannot be used as a changeable "TP" in initializer }
program T; type TP = class Age: Integer; Friend: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Age); end; function TP.Info: Integer; const; begin Result := Age; end;
procedure Z(P: TP const); var m: TP := P; begin  end;
begin end.
