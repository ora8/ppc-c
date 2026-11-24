{ EXPECT: use of 'a' after it may have been freed }
program T; type TFoo = class V: Integer; end;
var a: TFoo; val c: Boolean;
begin c := True; a := TFoo.Create; if c then a.Free; WriteLn(a.V); end.
