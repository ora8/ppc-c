{ EXPECT: 'Make' returns a reference to 'h', which is owned by local variable 'h' }
program T; type TFoo = class V: Integer; end;
function Make: borrowed TFoo; var h: TFoo.Create; begin Result := h; end;
begin end.
