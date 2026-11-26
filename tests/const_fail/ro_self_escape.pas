{* EXPECT: read-only reference ("TQ const") cannot be used as a changeable "TQ" in assignment *}
program T; type TP = class Age: Integer; Friend: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Age); end; function TP.Info: Integer; const; begin Result := Age; end;
type TQ = class function Me: var TQ; const; end; function TQ.Me: var TQ; const; begin Result := Self; end;
begin end.
