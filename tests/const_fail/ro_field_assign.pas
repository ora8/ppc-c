{ EXPECT: cannot modify a part of 'P', which is constant }
program T; type TP = class Age: Integer; Friend: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Age); end; function TP.Info: Integer; const; begin Result := Age; end;
procedure Z(P: TP const); begin P.Age := 1; end;
begin end.
