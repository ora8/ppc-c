{* EXPECT: "result" has no value: write 'ok' without an argument *}
program T;
function G: result; begin Result := ok(1); end;
begin end.
