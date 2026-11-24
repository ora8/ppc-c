{ EXPECT: 'a' may be used without an allocated object (it is not allocated on every path) }
program T; type TFoo = class V: Integer; Next: optional TFoo; end;
var a: TFoo; val c: Boolean;
begin c := False; if c then a := TFoo.Create; a.V := 1; a.Free; end.
