{ EXPECT: borrowed reference 'r' is used after its owner 'a' was freed }
program T; type TFoo = class V: Integer; end;
var a: TFoo; val r: borrowed TFoo;
begin a := TFoo.Create; r := a; a.Free; WriteLn(r.V); end.
