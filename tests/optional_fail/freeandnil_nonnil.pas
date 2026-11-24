{ EXPECT: FreeAndNil would leave non-nilable 'a' nil }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
var a: TFoo;
begin a := TFoo.Create; FreeAndNil(a); end.
