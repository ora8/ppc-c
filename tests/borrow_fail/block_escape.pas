{ EXPECT: borrowed reference 'r' is used after its owner 's' freed its object at the end of its block }
program T; type TFoo = class V: Integer; constructor Create; end; constructor TFoo.Create; begin inherited Create; end;
var r: optional TFoo := none;
begin vao s := TFoo.Create; begin  r := s; end; if r is some x then WriteLn(x.V); end.
