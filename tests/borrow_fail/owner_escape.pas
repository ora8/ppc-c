{ EXPECT: reference to 'h' escapes into 'G', but 'h' frees its object at the end of P }
program T; type TFoo = class V: Integer; end;
val G: borrowed optional TFoo;
procedure P; var h: TFoo.Create; begin G := h; end;
begin P; end.
