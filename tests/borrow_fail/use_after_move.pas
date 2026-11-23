{ EXPECT: use of moved value 'a' }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
heap a, b: TFoo;
begin a := TFoo.Create; b := a; a.V := 1; b.Free; end.
