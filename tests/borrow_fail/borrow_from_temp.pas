{ EXPECT: the result of 'Id' would borrow from a temporary object }
program T; type TFoo = class V: Integer; end;
function Id(x: TFoo): borrowed TFoo; begin Result := x; end;
val b: borrowed TFoo;
begin b := Id(TFoo.Create); end.
