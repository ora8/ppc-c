{ EXPECT: FreeAndNil would leave non-nilable 'a' nil }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
var a: TFoo;
begin a := TFoo.Create; FreeAndNil(a); end.
