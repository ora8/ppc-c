{ EXPECT: 'Make' returns nil, but its result type 'TFoo' is not nilable }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
function Make: TFoo; begin Result := nil; end;
begin end.
