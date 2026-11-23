{ EXPECT: use of 'a' after it may have been freed }
program T; type TFoo = class V: Integer; end; PInt = ^Integer;
var a: TFoo; c: Boolean;
begin c := True; a := TFoo.Create; if c then a.Free; WriteLn(a.V); end.
