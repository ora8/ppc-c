{* EXPECT: reference to 's' escapes into 'G', but 's' frees its object at the end of P *}
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
var G: optional TFoo := none;
procedure P; vao s := TFoo.Create(1); begin  G := s; end;
begin P; end.
