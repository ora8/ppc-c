{* EXPECT: 'r' is a reference and does not own its object, so it cannot be moved into stack object 's'; move it from its owner, or copy it ('val s := r.Clone') *}
program T;
type TFoo = class V: Integer; F: optional TFoo; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P; vao h := TFoo.Create(1); var r: TFoo := h; val s := r; begin end;
begin P; end.
