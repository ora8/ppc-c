{ EXPECT: 'try' cannot pass an error of type "String" on from a function whose errors are "TE" }
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
type TE = (A, B); function G: result Integer of TE; begin Result := try T2(1); end;
begin end.
