{ EXPECT: reference to local variable 's' escapes into 'G' }
program T; type TFoo = class V: Integer; constructor Create(AV: Integer); end; constructor TFoo.Create(AV: Integer); begin inherited Create; V := AV; end;
var G: borrowed nilable TFoo;
procedure P; var s: TFoo; begin s.Create(1); G := s; end;
begin P; end.
