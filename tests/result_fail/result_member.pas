{ EXPECT: 'r' is a result (result Integer): unpack it first, e.g. 'if r is ok v then ...' (members: IsOk, IsError, ValueOr) }
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
val r := T2(1);
begin WriteLn(r.Wert); end.
