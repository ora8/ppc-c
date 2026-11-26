{* EXPECT: name the value: 'is ok v' *}
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
val r := T2(1);
begin if r is ok then WriteLn(1); end.
