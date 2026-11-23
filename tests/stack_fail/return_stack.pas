{ EXPECT: 'Make' returns a reference to local variable 's' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
function Make: borrowed TFoo; var s: TFoo; begin s.Create(1); Result := s; end;
begin end.
