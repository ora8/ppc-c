{ EXPECT: cannot call 'Gb' on constant 'r': only const methods }
program T; type TP = class Alter: Integer; Freund: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Alter); end; function TP.Info: Integer; const; begin Result := Alter; end;
procedure Z(x: TP); val r: borrowed const TP; begin r := x; r.Gb; end;
begin end.
