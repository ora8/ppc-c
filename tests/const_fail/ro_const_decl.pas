{* EXPECT: cannot call 'Gb' on constant 'k' *}
program T; type TP = class Age: Integer; Friend: optional TP; procedure Gb; function Info: Integer; const; end; procedure TP.Gb; begin Inc(Age); end; function TP.Info: Integer; const; begin Result := Age; end;
procedure Z(x: TP); var k: TP const := x; begin k.Gb; end;
begin end.
