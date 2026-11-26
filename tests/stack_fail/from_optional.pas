{* EXPECT: the object for stack object 's' may be none ("optional TFoo"): unpack it first with 'is some', or use 'vao' *}
program T;
type TFoo = class V: Integer; F: optional TFoo; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; vao h: optional TFoo := none; val s := h; begin end;
begin P; end.
