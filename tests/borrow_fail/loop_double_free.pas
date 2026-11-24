{ EXPECT: 'a' is possibly freed twice (double free) }
program T; type TFoo = class V: Integer; end;
var a: TFoo; val i: Integer;
begin a := TFoo.Create; for i := 1 to 3 do a.Free; end.
