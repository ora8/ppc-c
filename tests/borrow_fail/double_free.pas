{ EXPECT: 'a' is freed twice (double free) }
program T; type TFoo = class V: Integer; end;
var a: TFoo;
begin a := TFoo.Create; a.Free; a.Free; end.
