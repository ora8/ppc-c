{ EXPECT: call by var for arg no. 1 has to match exactly }
program T; type TP = class Age: Integer; Friend: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Age); end; function TP.Info: Integer; const; begin Result := Age; end;
procedure V(var a: TP); begin end; procedure Z(P: TP const); var c: TP const := P; begin  V(c); end;
begin end.
