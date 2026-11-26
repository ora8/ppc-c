{* EXPECT: 'Make' returns a reference to 's', which is owned by local variable 's' *}
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
function Make: var TFoo; vao s := TFoo.Create(1); begin  Result := s; end;
begin end.
