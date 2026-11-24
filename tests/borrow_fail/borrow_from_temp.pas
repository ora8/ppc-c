{ EXPECT: the result of 'Id' would borrow from a temporary object }
program T; type TFoo = class V: Integer; end;
function Id(x: TFoo): var TFoo; begin Result := x; end;
var b: TFoo := Id(TFoo.Create);
begin  end.
