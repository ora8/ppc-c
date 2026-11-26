{* EXPECT: 'try' is only allowed as a whole statement, as the value of an assignment or as an initializer *}
program T;
function T2(n: Integer): result Integer; begin if n = 0 then Result := error('null') else Result := n; end;
function G: result Integer; begin Result := 1 + try T2(1); end;
begin end.
