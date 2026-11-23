{ EXPECT: use of 'a' after it was freed }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
heap a: TFoo;
begin a := TFoo.Create; a.Free; WriteLn(a.V); end.
