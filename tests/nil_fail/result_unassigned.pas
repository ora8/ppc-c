{ EXPECT: 'Make' never assigns an object to its result, but its result type 'TFoo' is not nilable }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
function Make: TFoo; begin end;
begin end.
