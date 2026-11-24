{ EXPECT: none can only be stored in an optional type, not in "TFoo" }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
var a: TFoo;
begin a := TFoo.Create; a.Free; a := none; end.
