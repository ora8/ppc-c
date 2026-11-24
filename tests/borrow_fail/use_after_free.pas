{ EXPECT: use of 'a' after it was freed }
program T; type TFoo = class V: Integer; end;
var a: TFoo;
begin a := TFoo.Create; a.Free; WriteLn(a.V); end.
