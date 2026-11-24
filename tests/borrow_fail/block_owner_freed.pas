{ EXPECT: borrowed reference 'r' is used after its owner 'h' freed its object at the end of its block }
program T; type TFoo = class V: Integer; end;
var r: optional TFoo := none;
begin vao h := TFoo.Create; begin r := h; end; if r is some x then WriteLn(x.V); end.
