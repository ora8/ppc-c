{ EXPECT: 'p' is used, but no object was allocated for it (missing 'New(p)') }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
var p: PInt;
begin p^ := 5; end.
