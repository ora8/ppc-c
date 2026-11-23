{ EXPECT: borrowed reference 'r' is used after its owner 'a' was freed }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
var a: TFoo; r: borrowed TFoo;
begin a := TFoo.Create; r := a; a.Free; WriteLn(r.V); end.
