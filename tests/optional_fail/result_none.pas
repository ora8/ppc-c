{ EXPECT: none can only be stored in an optional type, not in "TFoo" }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
function Make: TFoo; begin Result := none; end;
begin end.
