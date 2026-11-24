{ EXPECT: result case arms are 'ok v:' and 'error e:' (each at most once) }
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
begin case T2(1) of some v: WriteLn(v); none: WriteLn(0); end; end.
