{ EXPECT: 'a' is freed twice (double free) }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
heap a: TFoo;
begin a := TFoo.Create; a.Free; a.Free; end.
