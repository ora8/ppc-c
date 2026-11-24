{ EXPECT: 'a' is used, but no object was allocated for it (missing 'a := TFoo.Create') }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
var a: TFoo;
begin a.V := 1; end.
