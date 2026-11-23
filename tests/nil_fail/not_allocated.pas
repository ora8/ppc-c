{ EXPECT: 'a' is used, but no object was allocated for it (missing 'a := TFoo.Create') }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
heap a: TFoo;
begin a.V := 1; end.
