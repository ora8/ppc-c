{* EXPECT: result of type "result Integer" cannot be used as "Integer" in initializer; unpack it first: 'if r is ok v then ...', 'case r of ok v: ...; error e: ... end' or 'try r' *}
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
val x: Integer := T2(1);
begin end.
