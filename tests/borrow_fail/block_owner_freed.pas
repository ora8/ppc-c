{ EXPECT: borrowed reference 'r' is used after its owner 'h' freed its object at the end of its block }
program T; type TFoo = class V: Integer; end;
val r: borrowed optional TFoo;
begin var h: TFoo.Create; begin r := h; end; if r is some x then WriteLn(x.V); end.
