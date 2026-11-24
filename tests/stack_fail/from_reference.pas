{ EXPECT: 'x' is a reference and does not own its object, so it cannot be moved into stack object 's'; move it from its owner, or copy it ('val s := x.Clone') }
program T;
type TFoo = class V: Integer; F: optional TFoo; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
procedure P(x: TFoo); val s := x; begin end;
begin end.
