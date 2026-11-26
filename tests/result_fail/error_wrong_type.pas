{* EXPECT: incompatible types in error(...): got "Integer" expected "String" *}
program T;
function G: result Integer; begin Result := error(5); end;
begin end.
