{* EXPECT: use of moved value 'h' (ownership was transferred away) *}
program T;
type TFoo = class V: Integer; F: optional TFoo; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; vao h := TFoo.Create(1); val s := h; begin WriteLn(h.V); end;
begin P; end.
