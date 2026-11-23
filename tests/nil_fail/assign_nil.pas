{ EXPECT: cannot assign nil to non-nilable 'a' (declare it as 'nilable TFoo') }
program T; type TFoo = class V: Integer; Next: nilable TFoo; end; PInt = ^Integer;
var a: TFoo;
begin a := TFoo.Create; a.Free; a := nil; end.
